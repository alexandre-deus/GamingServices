// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaNetDriver.h"
#include "NetDriver/MinderaNetConnection.h"
#include "NetDriver/MinderaSocket.h"
#include "NetDriver/MinderaSocketSubsystem.h"
#include "Native/Interfaces/IP2PTransport.h"

#include "SocketSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MinderaNetDriver)

DEFINE_LOG_CATEGORY_STATIC(LogMinderaNet, Log, All);

UMinderaNetDriver::UMinderaNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

IP2PTransport* UMinderaNetDriver::GetTransport() const
{
	return FMinderaSocketSubsystem::GetTransport();
}

bool UMinderaNetDriver::IsP2PUrl(const FString& Host) const
{
	IP2PTransport* Transport = GetTransport();
	return Transport != nullptr && Host.StartsWith(Transport->GetUrlPrefix());
}

bool UMinderaNetDriver::IsAvailable() const
{
	IP2PTransport* Transport = GetTransport();
	return Transport != nullptr && Transport->IsAvailable();
}

ISocketSubsystem* UMinderaNetDriver::GetSocketSubsystem()
{
	if (bIsPassthrough)
	{
		return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	}
	return ISocketSubsystem::Get(MINDERA_SOCKET_SUBSYSTEM_NAME);
}

bool UMinderaNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	if (bIsPassthrough)
	{
		return UIpNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
	}
	// P2P path: the transport IS the socket, so we only need the UNetDriver base (no BSD socket / binds).
	return UNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
}

bool UMinderaNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		// PIE uses plain IP so local multiplayer works without the relay.
		bIsPassthrough = true;
		return Super::InitConnect(InNotify, ConnectURL, Error);
	}
#endif

	IP2PTransport* Transport = GetTransport();
	if (!Transport || !Transport->IsAvailable() || !IsP2PUrl(ConnectURL.Host))
	{
		UE_LOG(LogMinderaNet, Log, TEXT("[UMinderaNetDriver] InitConnect: IP passthrough (host '%s')"), *ConnectURL.Host);
		bIsPassthrough = true;
		return Super::InitConnect(InNotify, ConnectURL, Error);
	}

	// Resolve the peer id from the URL and open our channel.
	FString PeerId = ConnectURL.Host;
	PeerId.RemoveFromStart(Transport->GetUrlPrefix());
	if (PeerId.IsEmpty())
	{
		Error = FString::Printf(TEXT("MinderaNetDriver: could not parse peer id from '%s'"), *ConnectURL.Host);
		return false;
	}

	P2PSocket = MakeShareable(new FMinderaSocket(Transport, SOCKTYPE_Datagram, TEXT("Mindera Client Socket"), MINDERA_P2P_PROTOCOL));

	TSharedRef<FInternetAddrMindera> PeerAddr = MakeShareable(new FInternetAddrMindera());
	PeerAddr->SetPeerId(PeerId);
	PeerAddr->SetPort(P2PVirtualPort);
	P2PSocket->Connect(PeerAddr.Get());
	SetSocketAndLocalAddress(P2PSocket);

	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		P2PSocket.Reset();
		return false;
	}

	// The server connection: packets flow to PeerAddr through the socket/transport; the UE
	// connectionless handshake promotes it from Pending to Open.
	UMinderaNetConnection* ServerConn = NewObject<UMinderaNetConnection>(GetTransientPackage(), NetConnectionClass);
	check(ServerConn);
	ServerConn->bIsPassthrough = false;
	ServerConnection = ServerConn;
	ServerConnection->InitLocalConnection(this, P2PSocket.Get(), ConnectURL, USOCK_Pending);

	CreateInitialClientChannels();
	UE_LOG(LogMinderaNet, Log, TEXT("[UMinderaNetDriver] InitConnect: P2P connect to peer '%s' (vport %d)"), *PeerId, P2PVirtualPort);
	return true;
}

bool UMinderaNetDriver::InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		bIsPassthrough = true;
		return Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error);
	}
#endif

	IP2PTransport* Transport = GetTransport();
	if (!Transport || !Transport->IsAvailable())
	{
		bIsPassthrough = true;
		return Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error);
	}

	P2PSocket = MakeShareable(new FMinderaSocket(Transport, SOCKTYPE_Datagram, TEXT("Mindera Listen Socket"), MINDERA_P2P_PROTOCOL));

	TSharedRef<FInternetAddrMindera> ListenAddr = MakeShareable(new FInternetAddrMindera());
	ListenAddr->SetPeerId(Transport->GetLocalPeerId());
	ListenAddr->SetPort(P2PVirtualPort);
	P2PSocket->Bind(ListenAddr.Get());
	SetSocketAndLocalAddress(P2PSocket);

	// UIpNetDriver::InitListen sets up the ConnectionlessHandler + StatelessConnect handshake; our
	// InitBase override keeps it off the BSD socket path.
	if (!Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error))
	{
		P2PSocket.Reset();
		return false;
	}

	UE_LOG(LogMinderaNet, Log, TEXT("[UMinderaNetDriver] InitListen: P2P listen (vport %d, local peer %s)"), P2PVirtualPort, *Transport->GetLocalPeerId());
	return true;
}

void UMinderaNetDriver::CloseConnectionForPeer(const FString& PeerId)
{
	const auto Matches = [&PeerId](UNetConnection* Conn)
	{
		if (!Conn || !Conn->RemoteAddr.IsValid())
		{
			return false;
		}
		return static_cast<const FInternetAddrMindera&>(*Conn->RemoteAddr).GetPeerId() == PeerId;
	};

	for (int32 i = ClientConnections.Num() - 1; i >= 0; --i)
	{
		if (Matches(ClientConnections[i]))
		{
			UE_LOG(LogMinderaNet, Log, TEXT("[UMinderaNetDriver] Peer %s dropped, closing client connection"), *PeerId);
			ClientConnections[i]->Close();
		}
	}
	if (ServerConnection && Matches(ServerConnection))
	{
		UE_LOG(LogMinderaNet, Log, TEXT("[UMinderaNetDriver] Peer %s dropped, closing server connection"), *PeerId);
		ServerConnection->Close();
	}
}

void UMinderaNetDriver::TickDispatch(float DeltaTime)
{
	if (bIsPassthrough)
	{
		Super::TickDispatch(DeltaTime);
		return;
	}

	// Surface transport-level disconnects into UNetConnection closes (P2P has no OS reset packet).
	if (IP2PTransport* Transport = GetTransport())
	{
		Transport->Tick();
		TArray<FString> ClosedPeers;
		Transport->PumpClosedPeers(ClosedPeers);
		for (const FString& PeerId : ClosedPeers)
		{
			CloseConnectionForPeer(PeerId);
		}
	}

	// UIpNetDriver drains the datagram socket (RecvFrom -> transport) and dispatches by address /
	// through the connectionless handshake — identical to the plain IP path.
	Super::TickDispatch(DeltaTime);
}

void UMinderaNetDriver::Shutdown()
{
	Super::Shutdown();
	P2PSocket.Reset();
}

bool UMinderaNetDriver::IsNetResourceValid()
{
	if (bIsPassthrough)
	{
		return UIpNetDriver::IsNetResourceValid();
	}
	return P2PSocket.IsValid() && IsAvailable();
}

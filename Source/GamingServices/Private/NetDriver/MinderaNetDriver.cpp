// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaNetDriver.h"
#include "NetDriver/MinderaSocketSubsystem.h"
#include "Native/Interfaces/IP2PTransport.h"

#include "SocketSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MinderaNetDriver)

DEFINE_LOG_CATEGORY_STATIC(LogMinderaNet, Log, All);

namespace
{
	// Any non-zero value: it exists only so the address resolver's client bind socket reports success
	// (a port-0 bind is treated as failure). The connectionless transport ignores ports entirely.
	constexpr int32 GMinderaClientBindPort = 7777;
}

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
	return ISocketSubsystem::Get(bIsPassthrough ? PLATFORM_SOCKETSUBSYSTEM : MINDERA_SOCKET_SUBSYSTEM_NAME);
}

int UMinderaNetDriver::GetClientPort()
{
	return bIsPassthrough ? Super::GetClientPort() : GMinderaClientBindPort;
}

bool UMinderaNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	IP2PTransport* Transport = GetTransport();
	// Non-P2P URL (LAN / PIE / raw IP) or no transport: fall back to the platform socket subsystem.
	bIsPassthrough = !(Transport && Transport->IsAvailable() && IsP2PUrl(ConnectURL.Host));
	UE_LOG(LogMinderaNet, Log, TEXT("[UMinderaNetDriver] InitConnect: %s (host '%s')"),
	       bIsPassthrough ? TEXT("IP passthrough") : TEXT("P2P"), *ConnectURL.Host);

	return Super::InitConnect(InNotify, ConnectURL, Error);
}

bool UMinderaNetDriver::InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error)
{
	IP2PTransport* Transport = GetTransport();
	bIsPassthrough = !(Transport && Transport->IsAvailable() && !ListenURL.HasOption(TEXT("bIsLanMatch")));
	UE_LOG(LogMinderaNet, Log, TEXT("[UMinderaNetDriver] InitListen: %s"),
	       bIsPassthrough ? TEXT("IP passthrough") : TEXT("P2P"));

	return Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error);
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
	// Surface transport-level disconnects into UNetConnection closes (P2P has no OS reset packet).
	if (!bIsPassthrough)
	{
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
	}

	// UIpNetDriver drains the socket (RecvFrom -> transport) and dispatches by address / through the
	// connectionless handshake — identical to the plain IP path.
	Super::TickDispatch(DeltaTime);
}

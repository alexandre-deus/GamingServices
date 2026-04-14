// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaNetDriver.h"
#include "NetDriver/MinderaNetConnection.h"
#include "NetDriver/MinderaSocket.h"
#include "NetDriver/MinderaSocketSubsystem.h"
#include "NetDriver/MinderaInternetAddr.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingtypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MinderaNetDriver)

DEFINE_LOG_CATEGORY_STATIC(LogMinderaNet, Log, All);

static const TCHAR* MinderaSteamURLPrefix = TEXT("steam.");

UMinderaNetDriver* UMinderaNetDriver::sActiveCallbackDriver = nullptr;

// ---------------------------------------------------------------------------
UMinderaNetDriver::UMinderaNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] Constructor"));
}

void UMinderaNetDriver::PostInitProperties()
{
	UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] PostInitProperties"));
	Super::PostInitProperties();
}

// ---------------------------------------------------------------------------
// Availability
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::IsAvailable() const
{
	const bool bAvailable = SteamNetworkingSockets() != nullptr;
	UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] IsAvailable: %s"), bAvailable ? TEXT("true") : TEXT("false"));
	return bAvailable;
}

// ---------------------------------------------------------------------------
// Socket sub-system routing
// ---------------------------------------------------------------------------
ISocketSubsystem* UMinderaNetDriver::GetSocketSubsystem()
{
	if (bIsPassthrough)
	{
		UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] GetSocketSubsystem: using PLATFORM_SOCKETSUBSYSTEM (passthrough)"));
		return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	}
	UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] GetSocketSubsystem: using MinderaSteam"));
	return ISocketSubsystem::Get(MINDERA_SOCKET_SUBSYSTEM_NAME);
}

// ---------------------------------------------------------------------------
ISteamNetworkingSockets* UMinderaNetDriver::GetSteamSocketsInterface() const
{
	if (IsRunningDedicatedServer() && SteamGameServerNetworkingSockets())
	{
		UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] GetSteamSocketsInterface: using GameServer interface"));
		return SteamGameServerNetworkingSockets();
	}
	ISteamNetworkingSockets* Iface = SteamNetworkingSockets();
	UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] GetSteamSocketsInterface: using Client interface (ptr=%p)"), Iface);
	return Iface;
}

// ---------------------------------------------------------------------------
// Base initialisation
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify,
	const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitBase: bInitAsClient=%d, bIsPassthrough=%d, URL.Host='%s', URL.Port=%d"),
		(int32)bInitAsClient, (int32)bIsPassthrough, *URL.Host, URL.Port);

	if (bIsPassthrough)
	{
		UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] InitBase: delegating to UIpNetDriver (passthrough)"));
		return UIpNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
	}

	// Steam path: only need UNetDriver base init (no BSD socket)
	if (!UNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		UE_LOG(LogMinderaNet, Error, TEXT("[Driver] InitBase FAILED (Steam path): %s"), *Error);
		return false;
	}

	UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] InitBase: SUCCESS (Steam path)"));
	return true;
}

// ---------------------------------------------------------------------------
// Client – connect
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitConnect: Host='%s'"), *ConnectURL.Host);

	ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
	if (!Sockets || !ConnectURL.Host.StartsWith(MinderaSteamURLPrefix))
	{
		UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitConnect: falling back to IP passthrough"));
		bIsPassthrough = true;
		return Super::InitConnect(InNotify, ConnectURL, Error);
	}

	// Parse remote Steam ID
	FString RawId = ConnectURL.Host;
	RawId.RemoveFromStart(MinderaSteamURLPrefix);
	const uint64 RemoteSteamId64 = FCString::Atoi64(*RawId);
	if (RemoteSteamId64 == 0)
	{
		Error = FString::Printf(TEXT("MinderaNetDriver: could not parse Steam ID from '%s'"), *ConnectURL.Host);
		return false;
	}

	// Initialize relay network
	SteamNetworkingUtils()->InitRelayNetworkAccess();

	// Set up connection status callback
	sActiveCallbackDriver = this;
	SteamNetworkingConfigValue_t Opt;
	Opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
		(void*)&UMinderaNetDriver::SteamConnectionStatusChangedThunk);

	SteamNetworkingIdentity RemoteIdentity;
	RemoteIdentity.SetSteamID64(RemoteSteamId64);

	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitConnect: ConnectP2P to SteamID %llu (vport %d)"), RemoteSteamId64, SteamVirtualPort);
	HSteamNetConnection hConn = Sockets->ConnectP2P(RemoteIdentity, SteamVirtualPort, 1, &Opt);
	if (hConn == k_HSteamNetConnection_Invalid)
	{
		Error = TEXT("MinderaNetDriver: ConnectP2P failed");
		return false;
	}

	// Create our socket wrapper
	SteamSocket = MakeShareable(new FMinderaSocket(SOCKTYPE_Streaming, TEXT("Mindera Client Socket"), MINDERA_STEAM_P2P_PROTOCOL));
	SteamSocket->SetInternalHandle(hConn);

	// Register with UIpNetDriver so GetSocket() returns it
	SetSocketAndLocalAddress(SteamSocket);

	// Perform base init
	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		Sockets->CloseConnection(hConn, 0, "InitBase failed", false);
		SteamSocket.Reset();
		return false;
	}

	// Create server connection object
	UMinderaNetConnection* ServerConn = NewObject<UMinderaNetConnection>(GetTransientPackage());
	check(ServerConn);

	ServerConn->bIsPassthrough = false;
	ServerConn->SteamConnectionHandle = hConn;

	ServerConnection = ServerConn;
	ServerConnection->InitLocalConnection(this, SteamSocket.Get(), ConnectURL, USOCK_Pending);

	CreateInitialClientChannels();

	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitConnect: SUCCESS (SteamID=%llu, handle=%u)"), RemoteSteamId64, hConn);
	return true;
}

// ---------------------------------------------------------------------------
// Server – listen
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::InitListen(FNetworkNotify* InNotify, FURL& ListenURL,
	bool bReuseAddressAndPort, FString& Error)
{
	ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
	const bool bForceFallback = ListenURL.HasOption(TEXT("bIsLanMatch")) ||
		FParse::Param(FCommandLine::Get(), TEXT("forcepassthrough"));

	if (!Sockets || bForceFallback)
	{
		UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitListen: falling back to IP passthrough"));
		bIsPassthrough = true;
		return Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error);
	}

	SteamNetworkingUtils()->InitRelayNetworkAccess();

	sActiveCallbackDriver = this;
	SteamNetworkingConfigValue_t Opt;
	Opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
		(void*)&UMinderaNetDriver::SteamConnectionStatusChangedThunk);

	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitListen: CreateListenSocketP2P (vport %d)"), SteamVirtualPort);
	HSteamListenSocket ListenHandle = Sockets->CreateListenSocketP2P(SteamVirtualPort, 1, &Opt);
	if (ListenHandle == k_HSteamListenSocket_Invalid)
	{
		Error = TEXT("MinderaNetDriver: CreateListenSocketP2P failed");
		return false;
	}

	HSteamNetPollGroup PollGroupHandle = Sockets->CreatePollGroup();
	if (PollGroupHandle == k_HSteamNetPollGroup_Invalid)
	{
		Sockets->CloseListenSocket(ListenHandle);
		Error = TEXT("MinderaNetDriver: CreatePollGroup failed");
		return false;
	}

	// Create a listen socket wrapper
	SteamSocket = MakeShareable(new FMinderaSocket(SOCKTYPE_Streaming, TEXT("Mindera Listen Socket"), MINDERA_STEAM_P2P_PROTOCOL));
	SteamSocket->SetInternalHandle(ListenHandle);
	SteamSocket->SetPollGroup(PollGroupHandle);
	SteamSocket->SetIsListenSocket(true);

	// Register with UIpNetDriver so GetSocket() / TickDispatch can use it
	SetSocketAndLocalAddress(SteamSocket);

	// Call Super::InitListen (UIpNetDriver) which:
	// 1. Calls InitBase (our virtual override → UNetDriver::InitBase)
	// 2. Sets up ConnectionlessHandler + StatelessConnectComponent for the handshake flow
	// This matches the official USteamNetDriver pattern.
	if (!Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error))
	{
		Sockets->DestroyPollGroup(PollGroupHandle);
		Sockets->CloseListenSocket(ListenHandle);
		SteamSocket.Reset();
		return false;
	}

	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] InitListen: SUCCESS (listen=%u, poll=%u)"), ListenHandle, PollGroupHandle);
	return true;
}

// ---------------------------------------------------------------------------
// LowLevelSend (driver-level, used by connectionless packets)
// ---------------------------------------------------------------------------
void UMinderaNetDriver::LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	if (bIsPassthrough)
	{
		Super::LowLevelSend(Address, Data, CountBits, Traits);
		return;
	}

	if (!Address.IsValid())
	{
		UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] LowLevelSend: invalid address, dropping"));
		return;
	}

	const int32 DataLen = FMath::DivideAndRoundUp(CountBits, 8);
	if (DataLen <= 0) return;

	ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
	if (!Sockets)
	{
		UE_LOG(LogMinderaNet, Warning, TEXT("[Driver] LowLevelSend: Steam sockets interface is null"));
		return;
	}

	UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] LowLevelSend: %d bits (%d bytes) to %s"),
		CountBits, DataLen, *Address->ToString(true));

	// Search client connections for a matching address
	for (UNetConnection* ClientConn : ClientConnections)
	{
		UMinderaNetConnection* MC = Cast<UMinderaNetConnection>(ClientConn);
		if (MC && MC->SteamConnectionHandle != k_HSteamNetConnection_Invalid && MC->RemoteAddr.IsValid() && *MC->RemoteAddr == *Address)
		{
			Sockets->SendMessageToConnection(MC->SteamConnectionHandle, Data, static_cast<uint32>(DataLen),
				k_nSteamNetworkingSend_UnreliableNoNagle, nullptr);
			return;
		}
	}

	// Check server connection
	if (ServerConnection)
	{
		UMinderaNetConnection* SrvConn = Cast<UMinderaNetConnection>(ServerConnection);
		if (SrvConn && SrvConn->SteamConnectionHandle != k_HSteamNetConnection_Invalid)
		{
			Sockets->SendMessageToConnection(SrvConn->SteamConnectionHandle, Data, static_cast<uint32>(DataLen),
				k_nSteamNetworkingSend_UnreliableNoNagle, nullptr);
			return;
		}
	}

	// Fallback: check PendingSteamConnections for pre-handshake connections.
	// During the UE connectionless handshake, no UNetConnection exists yet but we
	// still need to send challenge responses back to the client via the Steam handle.
	const FInternetAddrMindera* SteamAddr = static_cast<const FInternetAddrMindera*>(Address.Get());
	if (SteamAddr)
	{
		SteamNetworkingIdentity Identity;
		Identity.SetSteamID64(SteamAddr->GetSteamID64());
		HSteamNetConnection Handle = FindSteamHandleForIdentity(Identity);
		if (Handle != k_HSteamNetConnection_Invalid)
		{
			UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] LowLevelSend: using pending handle=%u for %s"), Handle, *Address->ToString(true));
			Sockets->SendMessageToConnection(Handle, Data, static_cast<uint32>(DataLen),
				k_nSteamNetworkingSend_UnreliableNoNagle, nullptr);
			return;
		}
	}

	UE_LOG(LogMinderaNet, Warning, TEXT("[Driver] LowLevelSend: no matching connection for %s, dropping %d bytes"), *Address->ToString(true), DataLen);
}

// ---------------------------------------------------------------------------
// Tick – dispatch incoming messages
// ---------------------------------------------------------------------------
void UMinderaNetDriver::TickDispatch(float DeltaTime)
{
	if (bIsPassthrough)
	{
		Super::TickDispatch(DeltaTime);
		return;
	}

	ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
	if (!Sockets)
	{
		UE_LOG(LogMinderaNet, Warning, TEXT("[Driver] TickDispatch: Steam sockets interface is null, skipping"));
		return;
	}

	// Let Steam invoke connection-status callbacks (accept/connected/disconnected)
	Sockets->RunCallbacks();

	// Server path: let UIpNetDriver::TickDispatch handle everything.
	// It calls RecvFrom on our SteamSocket, which drains the poll group and populates
	// source addresses. UIpNetDriver routes packets to existing connections or through
	// the connectionless handshake handler for new connections — exactly like the official driver.
	if (SteamSocket && SteamSocket->IsListenSocket())
	{
		Super::TickDispatch(DeltaTime);
		return;
	}

	// Client path: UNetDriver base tick + drain messages from server connection
	UNetDriver::TickDispatch(DeltaTime);

	if (ServerConnection)
	{
		UMinderaNetConnection* SteamConn = Cast<UMinderaNetConnection>(ServerConnection);
		if (SteamConn && SteamConn->SteamConnectionHandle != k_HSteamNetConnection_Invalid)
		{
			constexpr int32 MaxMessages = 256;
			SteamNetworkingMessage_t* IncomingMessages[MaxMessages];
			const int32 MsgCount = Sockets->ReceiveMessagesOnConnection(
				SteamConn->SteamConnectionHandle, IncomingMessages, MaxMessages);

			for (int32 i = 0; i < MsgCount; ++i)
			{
				SteamNetworkingMessage_t* Msg = IncomingMessages[i];
				UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] TickDispatch: client received %d bytes from server"), Msg->m_cbSize);
				SteamConn->ReceivedRawPacket(const_cast<uint8*>(static_cast<const uint8*>(Msg->m_pData)), Msg->m_cbSize);
				Msg->Release();
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void UMinderaNetDriver::Shutdown()
{
	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] Shutdown: bIsPassthrough=%d, ClientConnections=%d, ServerConnection=%s, SteamSocket=%s"),
		(int32)bIsPassthrough, ClientConnections.Num(),
		ServerConnection ? TEXT("valid") : TEXT("null"),
		SteamSocket ? TEXT("valid") : TEXT("null"));

	// Official pattern: flush / switch to no-delay before tearing down.
	// For ISteamNetworkingSockets the equivalent is flushing each active connection.
	if (!bIsPassthrough)
	{
		ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
		if (Sockets)
		{
			for (UNetConnection* ClientConn : ClientConnections)
			{
				UMinderaNetConnection* MC = Cast<UMinderaNetConnection>(ClientConn);
				if (MC && MC->SteamConnectionHandle != k_HSteamNetConnection_Invalid)
				{
					UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] Shutdown: flushing messages on client handle=%u"), MC->SteamConnectionHandle);
					Sockets->FlushMessagesOnConnection(MC->SteamConnectionHandle);
				}
			}
			if (ServerConnection)
			{
				UMinderaNetConnection* SrvConn = Cast<UMinderaNetConnection>(ServerConnection);
				if (SrvConn && SrvConn->SteamConnectionHandle != k_HSteamNetConnection_Invalid)
				{
					UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] Shutdown: flushing messages on server handle=%u"), SrvConn->SteamConnectionHandle);
					Sockets->FlushMessagesOnConnection(SrvConn->SteamConnectionHandle);
				}
			}
		}
		else
		{
			UE_LOG(LogMinderaNet, Warning, TEXT("[Driver] Shutdown: Steam sockets interface is null, cannot flush"));
		}
	}

	if (sActiveCallbackDriver == this)
	{
		UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] Shutdown: clearing sActiveCallbackDriver"));
		sActiveCallbackDriver = nullptr;
	}

	if (SteamSocket)
	{
		UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] Shutdown: deleting SteamSocket (handle=%u, listen=%d)"),
			SteamSocket->GetInternalHandle(), (int32)SteamSocket->IsListenSocket());
		SteamSocket.Reset();
	}

	UE_LOG(LogMinderaNet, Verbose, TEXT("[Driver] Shutdown: calling Super::Shutdown"));
	Super::Shutdown();
	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] Shutdown: COMPLETE"));
}

// ---------------------------------------------------------------------------
bool UMinderaNetDriver::IsNetResourceValid()
{
	if (bIsPassthrough)
	{
		const bool bValid = UIpNetDriver::IsNetResourceValid();
		UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] IsNetResourceValid (passthrough): %s"), bValid ? TEXT("true") : TEXT("false"));
		return bValid;
	}
	// Mirror the official driver: check that both the socket and the Steam interface are valid.
	const bool bHasSocket = SteamSocket.IsValid();
	const bool bHasHandle = bHasSocket && SteamSocket->GetInternalHandle() != k_HSteamNetConnection_Invalid;
	const bool bHasInterface = GetSteamSocketsInterface() != nullptr;
	const bool bValid = bHasSocket && bHasHandle && bHasInterface;
	UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] IsNetResourceValid (Steam): %s (socket=%s, handle=%s, interface=%s)"),
		bValid ? TEXT("true") : TEXT("false"),
		bHasSocket ? TEXT("valid") : TEXT("null"),
		bHasHandle ? TEXT("valid") : TEXT("invalid"),
		bHasInterface ? TEXT("valid") : TEXT("null"));
	return bValid;
}

// ---------------------------------------------------------------------------
// Connection status callback
// ---------------------------------------------------------------------------
void UMinderaNetDriver::SteamConnectionStatusChangedThunk(SteamNetConnectionStatusChangedCallback_t* pInfo)
{
	if (sActiveCallbackDriver)
	{
		UE_LOG(LogMinderaNet, VeryVerbose, TEXT("[Driver] SteamConnectionStatusChangedThunk: routing to active driver"));
		sActiveCallbackDriver->OnConnectionStatusChanged(pInfo);
	}
	else
	{
		UE_LOG(LogMinderaNet, Warning, TEXT("[Driver] SteamConnectionStatusChangedThunk: no active driver, callback dropped (conn=%u)"),
			pInfo ? pInfo->m_hConn : 0);
	}
}

void UMinderaNetDriver::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo)
{
	ISteamNetworkingSockets* Sockets = GetSteamSocketsInterface();
	if (!Sockets) return;

	const ESteamNetworkingConnectionState NewState = pInfo->m_info.m_eState;
	const HSteamNetConnection hConn = pInfo->m_hConn;

	UE_LOG(LogMinderaNet, Log, TEXT("[Driver] ConnectionStatusChanged: conn=%u, %d -> %d, reason=%d"),
		hConn, (int32)pInfo->m_eOldState, (int32)NewState, pInfo->m_info.m_eEndReason);

	switch (NewState)
	{
	case k_ESteamNetworkingConnectionState_Connecting:
	{
		// Server side: incoming connection request — accept it and add to poll group.
		// Do NOT create a UNetConnection here. Let the packets flow through UIpNetDriver::TickDispatch
		// → RecvFrom → connectionless handshake → engine creates the connection after handshake.
		if (SteamSocket && SteamSocket->IsListenSocket())
		{
			EResult AcceptResult = Sockets->AcceptConnection(hConn);
			if (AcceptResult != k_EResultOK)
			{
				UE_LOG(LogMinderaNet, Error, TEXT("[Driver] AcceptConnection failed (%d) for conn=%u"), (int32)AcceptResult, hConn);
				Sockets->CloseConnection(hConn, 0, "AcceptConnection failed", false);
				break;
			}

			// Add to poll group so RecvFrom on SteamSocket picks up messages
			if (SteamSocket->GetPollGroup() != k_HSteamNetPollGroup_Invalid)
			{
				Sockets->SetConnectionPollGroup(hConn, SteamSocket->GetPollGroup());
			}

			// Store the mapping so InitRemoteConnection can set the Steam handle later
			PendingSteamConnections.Add(hConn, pInfo->m_info.m_identityRemote);

			UE_LOG(LogMinderaNet, Log, TEXT("[Driver] ACCEPTED Steam connection from SteamID %llu (conn=%u), waiting for UE handshake"),
				pInfo->m_info.m_identityRemote.GetSteamID64(), hConn);
		}
		break;
	}

	case k_ESteamNetworkingConnectionState_Connected:
	{
		// Client-side: promote ServerConnection from USOCK_Pending to USOCK_Open.
		UMinderaNetConnection* SrvConn = Cast<UMinderaNetConnection>(ServerConnection);
		if (SrvConn && SrvConn->SteamConnectionHandle == hConn)
		{
			UE_LOG(LogMinderaNet, Log, TEXT("[Driver] Server connection OPEN (handle=%u)"), hConn);
			SrvConn->SetConnectionState(USOCK_Open);
		}
		// Server-side: Steam handshake done. The UE connection may or may not exist yet
		// (created by the connectionless handshake flow). If it exists, ensure it's aware.
		// No explicit action needed — the UE handshake drives connection state.
		UE_LOG(LogMinderaNet, Log, TEXT("[Driver] Steam handshake completed for conn=%u"), hConn);
		break;
	}

	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
	{
		UE_LOG(LogMinderaNet, Warning, TEXT("[Driver] Connection LOST: conn=%u (state=%d, reason=%d, debug='%s')"),
			hConn, (int32)NewState, pInfo->m_info.m_eEndReason, UTF8_TO_TCHAR(pInfo->m_info.m_szEndDebug));

		// Steam API requires CloseConnection to free resources on ClosedByPeer/ProblemDetectedLocally.
		// Close it here immediately, then invalidate the handle on the UE connection so that
		// UMinderaNetConnection::CleanUp() (called later during GC) does not double-close.
		Sockets->CloseConnection(hConn, 0, nullptr, false);

		// Remove from pending map if the UE connection was never created
		PendingSteamConnections.Remove(hConn);

		for (int32 i = ClientConnections.Num() - 1; i >= 0; --i)
		{
			UMinderaNetConnection* MC = Cast<UMinderaNetConnection>(ClientConnections[i]);
			if (MC && MC->SteamConnectionHandle == hConn)
			{
				MC->SteamConnectionHandle = k_HSteamNetConnection_Invalid;
				MC->Close();
				break;
			}
		}

		if (ServerConnection)
		{
			UMinderaNetConnection* SrvConn = Cast<UMinderaNetConnection>(ServerConnection);
			if (SrvConn && SrvConn->SteamConnectionHandle == hConn)
			{
				SrvConn->SteamConnectionHandle = k_HSteamNetConnection_Invalid;
				SrvConn->Close();
			}
		}

		break;
	}

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
HSteamNetConnection UMinderaNetDriver::FindSteamHandleForIdentity(const SteamNetworkingIdentity& Identity) const
{
	for (const auto& Pair : PendingSteamConnections)
	{
		if (Pair.Value == Identity)
		{
			return Pair.Key;
		}
	}
	return k_HSteamNetConnection_Invalid;
}
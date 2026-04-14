#ifdef USE_STEAMWORKS

#include "NetDriver/MinderaNetDriver.h"
#include "NetDriver/MinderaNetConnection.h"
#include "SocketSubsystem.h"

#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"

DEFINE_LOG_CATEGORY_STATIC(LogMinderaNet, Log, All);

// Forward declaration of the connection status callback
static void SteamConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo);

// ============================================================================
// UMinderaNetConnection::FImpl — Steam backend
// ============================================================================

struct UMinderaNetConnection::FImpl
{
	HSteamNetConnection SteamConnectionHandle = k_HSteamNetConnection_Invalid;
	SteamNetworkingIdentity RemoteIdentity;
};

// ============================================================================
// UMinderaNetDriver::FImpl — Steam backend
// ============================================================================

struct UMinderaNetDriver::FImpl
{
	ISteamNetworkingSockets* SteamSocketsInterface = nullptr;
	HSteamListenSocket ListenSocketHandle = k_HSteamListenSocket_Invalid;
	HSteamNetPollGroup PollGroupHandle = k_HSteamNetPollGroup_Invalid;

	TMap<HSteamNetConnection, UMinderaNetConnection*> ConnectionMap;

	static UMinderaNetDriver* ActiveInstance;
};

UMinderaNetDriver* UMinderaNetDriver::FImpl::ActiveInstance = nullptr;

// ============================================================================
// UMinderaNetConnection — Steam implementation
// ============================================================================

UMinderaNetConnection::~UMinderaNetConnection() = default;

void UMinderaNetConnection::InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL,
                                                EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
	UE_LOG(LogMinderaNet, Log, TEXT("InitLocalConnection (listen server host)"));
}

void UMinderaNetConnection::InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL,
                                                  const FInternetAddr& InRemoteAddr, EConnectionState InState,
                                                  int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);
	UE_LOG(LogMinderaNet, Log, TEXT("InitRemoteConnection: %s"), *InURL.ToString());
}

void UMinderaNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	if (!Impl || Impl->SteamConnectionHandle == k_HSteamNetConnection_Invalid)
	{
		return;
	}

	const int32 CountBytes = FMath::DivideAndRoundUp(CountBits, 8);
	if (CountBytes <= 0)
	{
		return;
	}

	ISteamNetworkingSockets* Sockets = SteamNetworkingSockets();
	if (!Sockets)
	{
		return;
	}

	// UE manages its own reliability layer above the net driver, so we always send
	// unreliable with NoNagle for immediate dispatch
	const int32 SendFlags = k_nSteamNetworkingSend_Unreliable | k_nSteamNetworkingSend_NoNagle;

	EResult Result = Sockets->SendMessageToConnection(
		Impl->SteamConnectionHandle, Data, CountBytes, SendFlags, nullptr);
	if (Result != k_EResultOK)
	{
		UE_LOG(LogMinderaNet, Warning, TEXT("SendMessageToConnection failed: %d"), (int32)Result);
	}
}

FString UMinderaNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	if (!Impl)
	{
		return TEXT("steam.unknown");
	}

	char Buf[SteamNetworkingIdentity::k_cchMaxString];
	Impl->RemoteIdentity.ToString(Buf, sizeof(Buf));
	return FString::Printf(TEXT("steam.%s"), UTF8_TO_TCHAR(Buf));
}

FString UMinderaNetConnection::LowLevelDescribe()
{
	if (!Impl)
	{
		return TEXT("SteamP2P (no impl)");
	}
	return FString::Printf(TEXT("SteamP2P {Handle: %u, Remote: %s}"),
	                       Impl->SteamConnectionHandle, *LowLevelGetRemoteAddress());
}

void UMinderaNetConnection::CleanUp()
{
	if (Impl && Impl->SteamConnectionHandle != k_HSteamNetConnection_Invalid)
	{
		ISteamNetworkingSockets* Sockets = SteamNetworkingSockets();
		if (Sockets)
		{
			Sockets->CloseConnection(Impl->SteamConnectionHandle, 0, "UE Connection CleanUp", true);
		}
		Impl->SteamConnectionHandle = k_HSteamNetConnection_Invalid;
	}

	Super::CleanUp();
}

// ============================================================================
// Steam connection status callback (free function)
// ============================================================================

static void SteamConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo)
{
	UMinderaNetDriver* Driver = UMinderaNetDriver::FImpl::ActiveInstance;
	if (!Driver || !Driver->Impl || !Driver->Impl->SteamSocketsInterface)
	{
		return;
	}

	UMinderaNetDriver::FImpl* DImpl = Driver->Impl.Get();
	ISteamNetworkingSockets* Sockets = DImpl->SteamSocketsInterface;
	const HSteamNetConnection ConnHandle = pInfo->m_hConn;
	const SteamNetConnectionInfo_t& Info = pInfo->m_info;

	switch (Info.m_eState)
	{
	case k_ESteamNetworkingConnectionState_Connecting:
	{
		// Outbound connection we initiated (client-side) — ignore, wait for Connected
		if (Info.m_hListenSocket == k_HSteamListenSocket_Invalid)
		{
			break;
		}

		// Incoming connection on a listen socket — verify it's ours
		if (Info.m_hListenSocket != DImpl->ListenSocketHandle)
		{
			Sockets->CloseConnection(ConnHandle, 0, "Unknown listen socket", false);
			break;
		}

		// Ask the game if it accepts new connections
		if (!Driver->Notify->NotifyAcceptingConnection())
		{
			Sockets->CloseConnection(ConnHandle, 0, "Game not accepting connections", false);
			break;
		}

		// Accept the connection
		EResult AcceptResult = Sockets->AcceptConnection(ConnHandle);
		if (AcceptResult != k_EResultOK)
		{
			UE_LOG(LogMinderaNet, Warning, TEXT("AcceptConnection failed: %d"), (int32)AcceptResult);
			Sockets->CloseConnection(ConnHandle, 0, "Accept failed", false);
			break;
		}

		// Add to poll group for efficient batch message retrieval
		if (DImpl->PollGroupHandle != k_HSteamNetPollGroup_Invalid)
		{
			Sockets->SetConnectionPollGroup(ConnHandle, DImpl->PollGroupHandle);
		}

		// Create and initialize UE connection
		UMinderaNetConnection* NewConn = NewObject<UMinderaNetConnection>(GetTransientPackage());
		NewConn->Impl = MakeUnique<UMinderaNetConnection::FImpl>();
		NewConn->Impl->SteamConnectionHandle = ConnHandle;

		SteamNetConnectionInfo_t ConnInfo;
		if (Sockets->GetConnectionInfo(ConnHandle, &ConnInfo))
		{
			NewConn->Impl->RemoteIdentity = ConnInfo.m_identityRemote;
		}

		FURL ConnectionURL;
		ConnectionURL.Host = NewConn->LowLevelGetRemoteAddress();

		TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		NewConn->InitRemoteConnection(Driver, nullptr, ConnectionURL, *RemoteAddr, USOCK_Open);

		DImpl->ConnectionMap.Add(ConnHandle, NewConn);
		Driver->ClientConnections.Add(NewConn);
		Driver->Notify->NotifyAcceptedConnection(NewConn);

		UE_LOG(LogMinderaNet, Log, TEXT("Accepted Steam P2P connection (handle: %u)"), ConnHandle);
		break;
	}

	case k_ESteamNetworkingConnectionState_Connected:
	{
		UE_LOG(LogMinderaNet, Log, TEXT("Steam P2P connection established (handle: %u)"), ConnHandle);
		break;
	}

	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
	{
		UE_LOG(LogMinderaNet, Log,
		       TEXT("Steam P2P connection lost (handle: %u, reason: %d, debug: %s)"),
		       ConnHandle, Info.m_eEndReason, UTF8_TO_TCHAR(Info.m_szEndDebug));

		Sockets->CloseConnection(ConnHandle, 0, nullptr, false);

		UMinderaNetConnection* RemovedConn = nullptr;
		if (DImpl->ConnectionMap.RemoveAndCopyValue(ConnHandle, RemovedConn) && RemovedConn)
		{
			RemovedConn->Close();
		}
		break;
	}

	default:
		break;
	}
}

// ============================================================================
// UMinderaNetDriver — Steam implementation
// ============================================================================

UMinderaNetDriver::~UMinderaNetDriver() = default;

// ---------------------------------------------------------------------------
// InitBase
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL,
                                 bool bReuseAddressAndPort, FString& Error)
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	Impl = MakeUnique<FImpl>();

	Impl->SteamSocketsInterface = SteamNetworkingSockets();
	if (!Impl->SteamSocketsInterface)
	{
		Error = TEXT("SteamNetworkingSockets interface not available. Is Steam initialized?");
		UE_LOG(LogMinderaNet, Error, TEXT("%s"), *Error);
		Impl.Reset();
		return false;
	}

	// Initialize the Steam relay network for P2P / NAT traversal
	ISteamNetworkingUtils* Utils = SteamNetworkingUtils();
	if (Utils)
	{
		Utils->InitRelayNetworkAccess();
		Utils->SetGlobalCallback_SteamNetConnectionStatusChanged(&SteamConnectionStatusChangedCallback);
	}

	FImpl::ActiveInstance = this;

	UE_LOG(LogMinderaNet, Log, TEXT("MinderaNetDriver (Steamworks) initialized"));

	return true;
}

// ---------------------------------------------------------------------------
// InitConnect — client connecting to a server
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		return false;
	}

	// Parse Steam ID from URL host — expected format: steam.<steamid64>
	FString Host = ConnectURL.Host;
	if (!Host.StartsWith(TEXT("steam.")))
	{
		Error = FString::Printf(TEXT("Invalid Steam URL format '%s'. Expected 'steam.<steamid64>'"), *Host);
		UE_LOG(LogMinderaNet, Error, TEXT("%s"), *Error);
		return false;
	}

	FString SteamIdStr = Host.Mid(6); // skip "steam."
	uint64 SteamId64 = FCString::Strtoui64(*SteamIdStr, nullptr, 10);
	if (SteamId64 == 0)
	{
		Error = FString::Printf(TEXT("Invalid Steam ID in URL: '%s'"), *SteamIdStr);
		UE_LOG(LogMinderaNet, Error, TEXT("%s"), *Error);
		return false;
	}

	// Build identity and connect
	SteamNetworkingIdentity RemoteIdentity;
	RemoteIdentity.SetSteamID64(SteamId64);

	HSteamNetConnection ConnHandle = Impl->SteamSocketsInterface->ConnectP2P(RemoteIdentity, 0, 0, nullptr);
	if (ConnHandle == k_HSteamNetConnection_Invalid)
	{
		Error = TEXT("ConnectP2P failed — returned invalid connection handle");
		UE_LOG(LogMinderaNet, Error, TEXT("%s"), *Error);
		return false;
	}

	// Create UE connection to the server
	UMinderaNetConnection* ServerConn = NewObject<UMinderaNetConnection>(GetTransientPackage());
	ServerConn->Impl = MakeUnique<UMinderaNetConnection::FImpl>();
	ServerConn->Impl->SteamConnectionHandle = ConnHandle;
	ServerConn->Impl->RemoteIdentity = RemoteIdentity;

	FURL ConnectionURL;
	ConnectionURL.Host = Host;

	TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	ServerConn->InitRemoteConnection(this, nullptr, ConnectionURL, *RemoteAddr, USOCK_Open);

	Impl->ConnectionMap.Add(ConnHandle, ServerConn);
	ServerConnection = ServerConn;

	CreateInitialClientChannels();

	UE_LOG(LogMinderaNet, Log, TEXT("Connecting to Steam P2P host: %s (handle: %u)"), *Host, ConnHandle);

	return true;
}

// ---------------------------------------------------------------------------
// InitListen — server starting to listen for connections
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort,
                                   FString& Error)
{
	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	// Create a poll group for efficient message retrieval across all client connections
	Impl->PollGroupHandle = Impl->SteamSocketsInterface->CreatePollGroup();
	if (Impl->PollGroupHandle == k_HSteamNetPollGroup_Invalid)
	{
		Error = TEXT("Failed to create Steam poll group");
		UE_LOG(LogMinderaNet, Error, TEXT("%s"), *Error);
		return false;
	}

	// Create listen socket for P2P connections on virtual port 0
	Impl->ListenSocketHandle = Impl->SteamSocketsInterface->CreateListenSocketP2P(0, 0, nullptr);
	if (Impl->ListenSocketHandle == k_HSteamListenSocket_Invalid)
	{
		Impl->SteamSocketsInterface->DestroyPollGroup(Impl->PollGroupHandle);
		Impl->PollGroupHandle = k_HSteamNetPollGroup_Invalid;
		Error = TEXT("Failed to create Steam P2P listen socket");
		UE_LOG(LogMinderaNet, Error, TEXT("%s"), *Error);
		return false;
	}

	// Report the local Steam ID in the URL so clients know the address
	CSteamID LocalSteamId = SteamUser()->GetSteamID();
	LocalURL.Host = FString::Printf(TEXT("steam.%llu"), LocalSteamId.ConvertToUint64());

	UE_LOG(LogMinderaNet, Log, TEXT("Listening for Steam P2P connections at %s"), *LocalURL.Host);

	return true;
}

// ---------------------------------------------------------------------------
// TickDispatch — process incoming messages and callbacks
// ---------------------------------------------------------------------------
void UMinderaNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);

	if (!Impl || !Impl->SteamSocketsInterface)
	{
		return;
	}

	// Dispatch Steam callbacks, including connection status changes
	Impl->SteamSocketsInterface->RunCallbacks();

	// Receive and dispatch network messages
	static constexpr int32 MaxMessages = 256;
	SteamNetworkingMessage_t* Messages[MaxMessages];
	int32 NumMessages = 0;

	if (Impl->ListenSocketHandle != k_HSteamListenSocket_Invalid &&
	    Impl->PollGroupHandle != k_HSteamNetPollGroup_Invalid)
	{
		// Server: receive from all connections via poll group
		NumMessages = Impl->SteamSocketsInterface->ReceiveMessagesOnPollGroup(
			Impl->PollGroupHandle, Messages, MaxMessages);
	}
	else if (ServerConnection)
	{
		// Client: receive from the server connection only
		UMinderaNetConnection* SteamServerConn = Cast<UMinderaNetConnection>(ServerConnection);
		if (SteamServerConn && SteamServerConn->Impl)
		{
			HSteamNetConnection Handle = SteamServerConn->Impl->SteamConnectionHandle;
			if (Handle != k_HSteamNetConnection_Invalid)
			{
				NumMessages = Impl->SteamSocketsInterface->ReceiveMessagesOnConnection(
					Handle, Messages, MaxMessages);
			}
		}
	}

	for (int32 i = 0; i < NumMessages; ++i)
	{
		SteamNetworkingMessage_t* Msg = Messages[i];
		if (!Msg)
		{
			continue;
		}

		UMinderaNetConnection** FoundConn = Impl->ConnectionMap.Find(Msg->m_conn);
		if (FoundConn && *FoundConn)
		{
			const uint8* DataPtr = static_cast<const uint8*>(Msg->m_pData);
			const int32 DataSize = Msg->m_cbSize;

			if (DataPtr && DataSize > 0)
			{
				(*FoundConn)->ReceivedRawPacket(const_cast<uint8*>(DataPtr), DataSize);
			}
		}

		Msg->Release();
	}
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void UMinderaNetDriver::Shutdown()
{
	if (Impl)
	{
		if (Impl->SteamSocketsInterface)
		{
			// Close all tracked Steam connections
			for (auto& Pair : Impl->ConnectionMap)
			{
				Impl->SteamSocketsInterface->CloseConnection(Pair.Key, 0, "Driver shutdown", false);
			}
			Impl->ConnectionMap.Empty();

			if (Impl->ListenSocketHandle != k_HSteamListenSocket_Invalid)
			{
				Impl->SteamSocketsInterface->CloseListenSocket(Impl->ListenSocketHandle);
			}

			if (Impl->PollGroupHandle != k_HSteamNetPollGroup_Invalid)
			{
				Impl->SteamSocketsInterface->DestroyPollGroup(Impl->PollGroupHandle);
			}
		}

		if (FImpl::ActiveInstance == this)
		{
			FImpl::ActiveInstance = nullptr;

			ISteamNetworkingUtils* Utils = SteamNetworkingUtils();
			if (Utils)
			{
				Utils->SetGlobalCallback_SteamNetConnectionStatusChanged(nullptr);
			}
		}

		Impl.Reset();
	}

	UE_LOG(LogMinderaNet, Log, TEXT("MinderaNetDriver (Steamworks) shut down"));

	Super::Shutdown();
}

// ---------------------------------------------------------------------------
// IsNetResourceValid
// ---------------------------------------------------------------------------
bool UMinderaNetDriver::IsNetResourceValid()
{
	return Impl && Impl->SteamSocketsInterface != nullptr;
}

// ---------------------------------------------------------------------------
// GetSocketSubsystem
// ---------------------------------------------------------------------------
ISocketSubsystem* UMinderaNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
}

#endif // USE_STEAMWORKS

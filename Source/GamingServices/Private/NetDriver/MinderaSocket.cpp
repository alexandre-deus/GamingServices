// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaSocket.h"
#include "NetDriver/MinderaSocketSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogMinderaSocket, Log, All);

FMinderaSocket::FMinderaSocket(ESocketType InSocketType, const FString& InSocketDescription, const FName& InSocketProtocol)
	: FSocket(InSocketType, InSocketDescription, InSocketProtocol)
	, PollGroup(k_HSteamNetPollGroup_Invalid)
	, InternalHandle(k_HSteamNetConnection_Invalid)
	, SendMode(k_nSteamNetworkingSend_UnreliableNoNagle)
	, bShouldLingerOnClose(false)
	, bIsListenSocket(false)
	, bHasPendingData(false)
	, PendingData(nullptr)
{
	SocketSubsystem = static_cast<FMinderaSocketSubsystem*>(ISocketSubsystem::Get(MINDERA_SOCKET_SUBSYSTEM_NAME));
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Constructor: type=%d, desc='%s', protocol='%s'"),
		(int32)InSocketType, *InSocketDescription, *InSocketProtocol.ToString());
}

FMinderaSocket::~FMinderaSocket()
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Destructor: handle=%u, listen=%d, pollGroup=%u, hasPending=%d"),
		InternalHandle, (int32)bIsListenSocket, PollGroup, (int32)bHasPendingData);

	if (bHasPendingData && PendingData != nullptr)
	{
		PendingData->Release();
		PendingData = nullptr;
	}

	// Close the socket/connection first, then destroy the poll group.
	Close();

	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (PollGroup != k_HSteamNetPollGroup_Invalid && SocketInterface)
	{
		SocketInterface->DestroyPollGroup(PollGroup);
		PollGroup = k_HSteamNetPollGroup_Invalid;
	}
}

ISteamNetworkingSockets* FMinderaSocket::GetSteamSocketsInterface()
{
	if (IsRunningDedicatedServer() && SteamGameServerNetworkingSockets())
	{
		return SteamGameServerNetworkingSockets();
	}
	return SteamNetworkingSockets();
}

bool FMinderaSocket::Close()
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Close: handle=%u, listen=%d"), InternalHandle, (int32)bIsListenSocket);

	// Both k_HSteamNetConnection_Invalid and k_HSteamListenSocket_Invalid are 0
	if (InternalHandle == k_HSteamNetConnection_Invalid)
	{
		UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Close: handle already invalid, nothing to do"));
		return true;
	}

	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (!SocketInterface)
	{
		return false;
	}

	bool bSuccess = false;
	if (bIsListenSocket)
	{
		bSuccess = SocketInterface->CloseListenSocket(InternalHandle);
		if (bSuccess)
		{
			UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Close: closed listen socket %u successfully"), InternalHandle);
			InternalHandle = k_HSteamListenSocket_Invalid;
		}
		else
		{
			UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Close: failed to close listen socket %u"), InternalHandle);
		}
	}
	else
	{
		bSuccess = SocketInterface->CloseConnection(InternalHandle, k_ESteamNetConnectionEnd_App_Generic, "Connection Ended.", bShouldLingerOnClose);
		if (bSuccess)
		{
			UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Close: closed connection %u successfully (linger=%d)"), InternalHandle, (int32)bShouldLingerOnClose);
			InternalHandle = k_HSteamNetConnection_Invalid;
		}
		else
		{
			UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Close: failed to close connection %u"), InternalHandle);
		}
	}

	return bSuccess;
}

bool FMinderaSocket::Bind(const FInternetAddr& Addr)
{
	BindAddress = *static_cast<const FInternetAddrMindera*>(&Addr);
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Bind: addr=%s"), *Addr.ToString(true));
	return true;
}

bool FMinderaSocket::Connect(const FInternetAddr& Addr)
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Connect: target=%s"), *Addr.ToString(true));

	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (!SocketInterface)
	{
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Connect: socket interface is null"));
		return false;
	}

	if (!Addr.IsValid())
	{
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Connect: address is not valid"));
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EINVAL;
		return false;
	}

	const FInternetAddrMindera& SteamAddr = *static_cast<const FInternetAddrMindera*>(&Addr);

	if (SteamAddr.IsIPAddress())
	{
		SteamNetworkingIPAddr IPAddr = SteamAddr;
		UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Connect: using ConnectByIPAddress"));
		InternalHandle = SocketInterface->ConnectByIPAddress(IPAddr, 0, nullptr);
	}
	else
	{
		SteamNetworkingIdentity Identity = SteamAddr;
		UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Connect: using ConnectP2P (SteamID=%llu, port=%d)"),
			SteamAddr.GetSteamID64(), SteamAddr.GetPort());
		InternalHandle = SocketInterface->ConnectP2P(Identity, SteamAddr.GetPort(), 0, nullptr);
	}

	if (InternalHandle != k_HSteamNetConnection_Invalid)
	{
		UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Connect: SUCCESS to %s (handle=%u)"), *Addr.ToString(true), InternalHandle);
		return true;
	}

	UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Connect: FAILED to %s"), *Addr.ToString(true));
	return false;
}

bool FMinderaSocket::Listen(int32 MaxBacklog)
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Listen: MaxBacklog=%d, BindAddr=%s"),
		MaxBacklog, *BindAddress.ToString(true));

	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (!SocketInterface)
	{
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Listen: socket interface is null"));
		return false;
	}

	bIsListenSocket = true;

	if (BindAddress.IsIPAddress())
	{
		SteamNetworkingIPAddr IPAddr = BindAddress;
		UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Listen: using CreateListenSocketIP"));
		InternalHandle = SocketInterface->CreateListenSocketIP(IPAddr, 0, nullptr);
	}
	else
	{
		UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Listen: using CreateListenSocketP2P (vport=%d)"), BindAddress.GetPlatformPort());
		InternalHandle = SocketInterface->CreateListenSocketP2P(BindAddress.GetPlatformPort(), 0, nullptr);
	}

	if (InternalHandle != k_HSteamListenSocket_Invalid)
	{
		PollGroup = SocketInterface->CreatePollGroup();
		UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Listen: SUCCESS (handle=%u, pollGroup=%u)"), InternalHandle, PollGroup);
		return true;
	}

	UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Listen: FAILED to create listen socket"));
	bIsListenSocket = false;
	return false;
}

FSocket* FMinderaSocket::Accept(const FString& InSocketDescription)
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Accept(desc): creating child socket '%s'"), *InSocketDescription);
	FMinderaSocket* NewSocket = new FMinderaSocket(SOCKTYPE_Streaming, InSocketDescription, GetProtocol());
	NewSocket->SendMode = SendMode;
	NewSocket->bShouldLingerOnClose = bShouldLingerOnClose;
	NewSocket->BindAddress.SetPlatformPort(BindAddress.GetPlatformPort());
	return NewSocket;
}

FSocket* FMinderaSocket::Accept(FInternetAddr& OutAddr, const FString& InSocketDescription)
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] Accept(addr,desc): not supported, returning nullptr"));
	return nullptr;
}

bool FMinderaSocket::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	// For a listen socket, sends are routed by the driver (UMinderaNetDriver::LowLevelSend)
	// which maps destination addresses to specific Steam connection handles.
	// SendTo on the listen socket itself can't work because it has no per-client handle.
	if (bIsListenSocket)
	{
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] SendTo: listen socket, sends handled by driver"));
		BytesSent = 0;
		return false;
	}
	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] SendTo: %d bytes to %s (delegating to Send)"), Count, *Destination.ToString(true));
	return Send(Data, Count, BytesSent);
}

bool FMinderaSocket::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	BytesSent = 0;

	if (InternalHandle == k_HSteamNetConnection_Invalid)
	{
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] Send: handle invalid, skipping"));
		return false;
	}

	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] Send: %d bytes on handle=%u, sendMode=%d"), Count, InternalHandle, SendMode);
	EResult Result = SocketInterface->SendMessageToConnection(InternalHandle, Data, static_cast<uint32>(Count), SendMode, nullptr);

	switch (Result)
	{
	case k_EResultOK:
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_NO_ERROR;
		BytesSent = Count;
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] Send: SUCCESS (%d bytes)"), Count);
		return true;
	case k_EResultInvalidParam:
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Send: FAILED with InvalidParam"));
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EINVAL;
		break;
	case k_EResultInvalidState:
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Send: FAILED with InvalidState"));
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EBADF;
		break;
	case k_EResultNoConnection:
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Send: FAILED with NoConnection"));
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_ENOTCONN;
		break;
	case k_EResultLimitExceeded:
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Send: FAILED with LimitExceeded"));
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EPROCLIM;
		break;
	default:
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Send: FAILED with unknown result %d"), (int32)Result);
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EFAULT;
		break;
	}

	BytesSent = -1;
	return false;
}

bool FMinderaSocket::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	BytesRead = -1;
	SteamNetworkingMessage_t* Message = nullptr;
	int32 MessagesRead = 0;

	if (!RecvRaw(Message, 1, MessagesRead, Flags))
	{
		return false;
	}

	if (MessagesRead < 1 || Message == nullptr)
	{
		// No data available — match official FSocketSteam::RecvFrom behavior.
		BytesRead = 0;
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EWOULDBLOCK;
		return false;
	}

	const int32 MsgSize = Message->m_cbSize;
	if (BufferSize < 0 || MsgSize > BufferSize)
	{
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] RecvFrom: message too large (%d > buffer %d)"), MsgSize, BufferSize);
		Message->Release();
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EMSGSIZE;
		return false;
	}

	FMemory::Memcpy(Data, Message->m_pData, MsgSize);
	BytesRead = MsgSize;

	// Populate Source address from the Steam message's connection identity
	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (SocketInterface)
	{
		SteamNetConnectionInfo_t ConnInfo;
		if (SocketInterface->GetConnectionInfo(Message->m_conn, &ConnInfo))
		{
			FInternetAddrMindera& SteamSource = static_cast<FInternetAddrMindera&>(Source);
			SteamSource.SetIdentity(ConnInfo.m_identityRemote);
			// Match official FSocketSteam::RecvFrom: set port to bound channel so addresses
			// match what connections expect (e.g. ServerConnection->RemoteAddr uses URL.Port).
			SteamSource.SetPort(BindAddress.GetPort());
		}
	}

	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] RecvFrom: received %d bytes from %s"), MsgSize, *Source.ToString(true));

	Message->Release();
	return true;
}

bool FMinderaSocket::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] Recv: bufSize=%d, flags=%d"), BufferSize, (int32)Flags);
	BytesRead = -1;
	SteamNetworkingMessage_t* Message = nullptr;
	int32 MessagesRead = 0;

	if (RecvRaw(Message, 1, MessagesRead, Flags))
	{
		if (MessagesRead >= 1 && Message != nullptr)
		{
			const int32 MsgSize = Message->m_cbSize;
			if (BufferSize >= 0 && MsgSize <= BufferSize)
			{
				FMemory::Memcpy(Data, Message->m_pData, MsgSize);
				BytesRead = MsgSize;
				UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] Recv: received %d bytes"), MsgSize);
				Message->Release();
			}
			else
			{
				UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] Recv: message too large (%d > buffer %d)"), MsgSize, BufferSize);
				BytesRead = -1;
				Message->Release();
				if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EMSGSIZE;
				return false;
			}
		}
		else
		{
			// No data available
			BytesRead = 0;
			if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EWOULDBLOCK;
			return false;
		}
	}

	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] Recv: RecvRaw returned false"));
	return false;
}

bool FMinderaSocket::RecvRaw(SteamNetworkingMessage_t*& OutData, int32 MaxMessages, int32& MessagesRead, ESocketReceiveFlags::Type Flags)
{
	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (!SocketInterface)
	{
		UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] RecvRaw: socket interface null"));
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_SYSNOTREADY;
		return false;
	}

	if (InternalHandle == k_HSteamNetConnection_Invalid)
	{
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] RecvRaw: handle invalid"));
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EINVAL;
		return false;
	}

	const bool bIsPeeking = (Flags == ESocketReceiveFlags::Peek);
	if (bIsPeeking)
	{
		MaxMessages = 1;
		if (bHasPendingData)
		{
			MessagesRead = 1;
			return true;
		}
	}
	else if (bHasPendingData && PendingData != nullptr)
	{
		OutData = PendingData;
		PendingData = nullptr;
		MessagesRead = 1;
		bHasPendingData = false;
		return true;
	}

	SteamNetworkingMessage_t** Target = bIsPeeking ? &PendingData : &OutData;
	MessagesRead = bIsListenSocket
		? SocketInterface->ReceiveMessagesOnPollGroup(PollGroup, Target, MaxMessages)
		: SocketInterface->ReceiveMessagesOnConnection(InternalHandle, Target, MaxMessages);

	if (MessagesRead >= 1)
	{
		if (bIsPeeking) bHasPendingData = true;
		if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_NO_ERROR;
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] RecvRaw: got %d message(s), peek=%d"), MessagesRead, (int32)bIsPeeking);
		return true;
	}
	else if (MessagesRead == 0)
	{
		bHasPendingData = false;
		PendingData = nullptr;
		return true;
	}

	UE_LOG(LogMinderaSocket, Warning, TEXT("[Socket] RecvRaw: error, returned %d"), MessagesRead);

	if (SocketSubsystem) SocketSubsystem->LastSocketError = SE_EFAULT;
	return false;
}

bool FMinderaSocket::HasPendingData(uint32& PendingDataSize)
{
	if (bHasPendingData && PendingData != nullptr)
	{
		PendingDataSize = static_cast<uint32>(PendingData->m_cbSize);
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] HasPendingData: yes (cached), size=%u"), PendingDataSize);
		return true;
	}

	int32 MessagesRead = 0;
	PendingDataSize = 0;
	SteamNetworkingMessage_t* FakeMessage = nullptr;

	if (RecvRaw(FakeMessage, 1, MessagesRead, ESocketReceiveFlags::Peek))
	{
		if (MessagesRead >= 1 && PendingData != nullptr)
		{
			PendingDataSize = static_cast<uint32>(PendingData->m_cbSize);
			UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] HasPendingData: yes (peeked), size=%u"), PendingDataSize);
			return true;
		}
	}

	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] HasPendingData: no"));
	return false;
}

ESocketConnectionState FMinderaSocket::GetConnectionState()
{
	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	if (InternalHandle == k_HSteamNetConnection_Invalid || !SocketInterface)
	{
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] GetConnectionState: not connected (handle=%u, iface=%p)"),
			InternalHandle, SocketInterface);
		return SCS_NotConnected;
	}

	SteamNetConnectionRealTimeStatus_t Status;
	if (SocketInterface->GetConnectionRealTimeStatus(InternalHandle, &Status, 0, nullptr) == k_EResultOK)
	{
		UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] GetConnectionState: handle=%u, steamState=%d"),
			InternalHandle, (int32)Status.m_eState);
		switch (Status.m_eState)
		{
		case k_ESteamNetworkingConnectionState_Connected:
			return SCS_Connected;
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			return SCS_ConnectionError;
		default:
			return SCS_NotConnected;
		}
	}

	return SCS_NotConnected;
}

void FMinderaSocket::GetAddress(FInternetAddr& OutAddr)
{
	FInternetAddrMindera& SteamAddr = static_cast<FInternetAddrMindera&>(OutAddr);
	SteamAddr = BindAddress;
	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] GetAddress: %s"), *BindAddress.ToString(true));
}

bool FMinderaSocket::GetPeerAddress(FInternetAddr& OutAddr)
{
	ISteamNetworkingSockets* SocketInterface = GetSteamSocketsInterface();
	FInternetAddrMindera& SteamAddr = static_cast<FInternetAddrMindera&>(OutAddr);

	SteamNetConnectionInfo_t ConnectionInfo;
	if (SocketInterface && SocketInterface->GetConnectionInfo(InternalHandle, &ConnectionInfo))
	{
		if (!ConnectionInfo.m_identityRemote.IsInvalid())
		{
			SteamAddr.SetIdentity(ConnectionInfo.m_identityRemote);
			SteamAddr.SetPlatformPort(BindAddress.GetPlatformPort());
			UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] GetPeerAddress: identity=%s"), *SteamAddr.ToString(true));
			return true;
		}
		if (!ConnectionInfo.m_addrRemote.IsIPv6AllZeros())
		{
			SteamAddr = FInternetAddrMindera(ConnectionInfo.m_addrRemote);
			UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] GetPeerAddress: ip=%s"), *SteamAddr.ToString(true));
			return true;
		}
	}

	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] GetPeerAddress: failed to get peer (handle=%u)"), InternalHandle);
	return false;
}

bool FMinderaSocket::SetNoDelay(bool bIsNoDelay)
{
	const int32 OldMode = SendMode;
	if (bIsNoDelay)
	{
		if (SendMode == k_nSteamNetworkingSend_Unreliable || SendMode == k_nSteamNetworkingSend_NoNagle ||
			SendMode == k_nSteamNetworkingSend_UnreliableNoNagle)
		{
			SendMode = k_nSteamNetworkingSend_UnreliableNoDelay;
		}
		else if (SendMode == k_nSteamNetworkingSend_Reliable)
		{
			SendMode = k_nSteamNetworkingSend_ReliableNoNagle;
		}
	}
	else
	{
		if (SendMode == k_nSteamNetworkingSend_NoDelay || SendMode == k_nSteamNetworkingSend_UnreliableNoDelay)
		{
			SendMode = k_nSteamNetworkingSend_UnreliableNoNagle;
		}
		else if (SendMode == k_nSteamNetworkingSend_ReliableNoNagle)
		{
			SendMode = k_nSteamNetworkingSend_Reliable;
		}
	}
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] SetNoDelay: %d, SendMode %d -> %d"), (int32)bIsNoDelay, OldMode, SendMode);
	return true;
}

bool FMinderaSocket::SetLinger(bool bShouldLinger, int32 Timeout)
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] SetLinger: linger=%d, timeout=%d"), (int32)bShouldLinger, Timeout);
	bShouldLingerOnClose = bShouldLinger;
	return true;
}

bool FMinderaSocket::SetSendBufferSize(int32 Size, int32& NewSize)
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] SetSendBufferSize: requested=%d"), Size);
	NewSize = Size;
	return true;
}

bool FMinderaSocket::SetReceiveBufferSize(int32 Size, int32& NewSize)
{
	UE_LOG(LogMinderaSocket, Verbose, TEXT("[Socket] SetReceiveBufferSize: requested=%d"), Size);
	NewSize = Size;
	return true;
}

int32 FMinderaSocket::GetPortNo()
{
	const int32 Port = BindAddress.GetPort();
	UE_LOG(LogMinderaSocket, VeryVerbose, TEXT("[Socket] GetPortNo: %d"), Port);
	return Port;
}
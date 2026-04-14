// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"

#include "NetDriver/MinderaInternetAddr.h"

class FMinderaSocketSubsystem;

/**
 * Socket wrapper around a Steam Networking Sockets connection or listen socket.
 * Uses the newer ISteamNetworkingSockets API with P2P relay support.
 */
class FMinderaSocket : public FSocket
{
public:
	FMinderaSocket(ESocketType InSocketType, const FString& InSocketDescription, const FName& InSocketProtocol);
	virtual ~FMinderaSocket();

	// -- FSocket interface --
	virtual bool Close() override;
	virtual bool Bind(const FInternetAddr& Addr) override;
	virtual bool Connect(const FInternetAddr& Addr) override;
	virtual bool Listen(int32 MaxBacklog) override;

	virtual class FSocket* Accept(const FString& InSocketDescription) override;
	virtual class FSocket* Accept(FInternetAddr& OutAddr, const FString& InSocketDescription) override;

	virtual bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination) override;
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override;

	virtual bool RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override;
	virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override;

	virtual bool HasPendingData(uint32& PendingDataSize) override;
	virtual ESocketConnectionState GetConnectionState() override;

	virtual void GetAddress(FInternetAddr& OutAddr) override;
	virtual bool GetPeerAddress(FInternetAddr& OutAddr) override;

	virtual bool SetNoDelay(bool bIsNoDelay = true) override;
	virtual bool SetLinger(bool bShouldLinger = true, int32 Timeout = 0) override;
	virtual bool SetSendBufferSize(int32 Size, int32& NewSize) override;
	virtual bool SetReceiveBufferSize(int32 Size, int32& NewSize) override;
	virtual int32 GetPortNo() override;

	// -- Unsupported operations --
	virtual bool Shutdown(ESocketShutdownMode Mode) override { return false; }
	virtual bool Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime) override { return false; }
	virtual bool WaitForPendingConnection(bool& bHasPendingConnection, const FTimespan& WaitTime) override { return false; }
	virtual bool SetReuseAddr(bool bAllowReuse = true) override { return true; }
	virtual bool SetRecvErr(bool bUseErrorQueue = true) override { return true; }
	virtual bool SetNonBlocking(bool bIsNonBlocking = true) override { return true; }
	virtual bool SetBroadcast(bool bAllowBroadcast = true) override { return true; }
	virtual bool JoinMulticastGroup(const FInternetAddr& GroupAddress) override { return false; }
	virtual bool JoinMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress) override { return false; }
	virtual bool LeaveMulticastGroup(const FInternetAddr& GroupAddress) override { return false; }
	virtual bool LeaveMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress) override { return false; }
	virtual bool SetMulticastLoopback(bool bLoopback) override { return false; }
	virtual bool SetMulticastTtl(uint8 TimeToLive) override { return false; }
	virtual bool SetMulticastInterface(const FInternetAddr& InterfaceAddress) override { return false; }

	// -- Steam-specific accessors --
	HSteamNetConnection GetInternalHandle() const { return InternalHandle; }
	HSteamNetPollGroup GetPollGroup() const { return PollGroup; }
	bool IsListenSocket() const { return bIsListenSocket; }

	void SetInternalHandle(HSteamNetConnection InHandle) { InternalHandle = InHandle; }
	void SetPollGroup(HSteamNetPollGroup InPollGroup) { PollGroup = InPollGroup; }
	void SetIsListenSocket(bool bListen) { bIsListenSocket = bListen; }
	void SetSendMode(int32 NewSendMode) { SendMode = NewSendMode; }

	/** Receives raw Steam messages. Use for advanced polling scenarios. */
	bool RecvRaw(SteamNetworkingMessage_t*& OutData, int32 MaxMessages, int32& MessagesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None);

private:
	/** Returns the active ISteamNetworkingSockets interface (client or game server). */
	static ISteamNetworkingSockets* GetSteamSocketsInterface();

	HSteamNetPollGroup PollGroup;
	HSteamNetConnection InternalHandle;
	FInternetAddrMindera BindAddress;
	int32 SendMode;
	bool bShouldLingerOnClose;
	bool bIsListenSocket;
	bool bHasPendingData;
	SteamNetworkingMessage_t* PendingData;

	FMinderaSocketSubsystem* SocketSubsystem;
};
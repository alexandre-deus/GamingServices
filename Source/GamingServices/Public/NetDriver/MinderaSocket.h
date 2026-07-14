// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"

#include "NetDriver/MinderaInternetAddr.h"

class IP2PTransport;

/**
 * Datagram socket over an IP2PTransport. Connectionless like a UDP socket: SendTo/RecvFrom address
 * peers by id on a virtual-port channel, so UIpNetDriver drives it exactly as it would a UDP socket.
 * Fully SDK-free — all platform networking lives behind the transport.
 */
class FMinderaSocket : public FSocket
{
public:
	FMinderaSocket(IP2PTransport* InTransport, ESocketType InSocketType, const FString& InSocketDescription, const FName& InSocketProtocol);
	virtual ~FMinderaSocket();

	// -- FSocket interface --
	virtual bool Close() override;
	virtual bool Bind(const FInternetAddr& Addr) override;
	virtual bool Connect(const FInternetAddr& Addr) override;
	virtual bool Listen(int32 MaxBacklog) override;

	virtual class FSocket* Accept(const FString& InSocketDescription) override { return nullptr; }
	virtual class FSocket* Accept(FInternetAddr& OutAddr, const FString& InSocketDescription) override { return nullptr; }

	virtual bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination) override;
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override;

	virtual bool RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override;
	virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override;

	virtual bool HasPendingData(uint32& PendingDataSize) override;
	virtual ESocketConnectionState GetConnectionState() override;

	virtual void GetAddress(FInternetAddr& OutAddr) override;
	virtual bool GetPeerAddress(FInternetAddr& OutAddr) override;

	virtual bool SetNoDelay(bool bIsNoDelay = true) override { return true; }
	virtual bool SetLinger(bool bShouldLinger = true, int32 Timeout = 0) override { return true; }
	virtual bool SetSendBufferSize(int32 Size, int32& NewSize) override { NewSize = Size; return true; }
	virtual bool SetReceiveBufferSize(int32 Size, int32& NewSize) override { NewSize = Size; return true; }
	virtual int32 GetPortNo() override { return Channel; }

	// -- Unsupported operations (connectionless P2P) --
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

	int32 GetChannel() const { return Channel; }

private:
	IP2PTransport* Transport = nullptr;
	int32 Channel = 0;
	FInternetAddrMindera BoundAddr;
	FInternetAddrMindera PeerAddr;
};

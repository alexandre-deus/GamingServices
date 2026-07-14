// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaSocket.h"
#include "Native/Interfaces/IP2PTransport.h"

// SDK-free: everything routes through IP2PTransport.

FMinderaSocket::FMinderaSocket(IP2PTransport* InTransport, ESocketType InSocketType, const FString& InSocketDescription, const FName& InSocketProtocol)
	: FSocket(InSocketType, InSocketDescription, InSocketProtocol)
	, Transport(InTransport)
{
}

FMinderaSocket::~FMinderaSocket()
{
	Close();
}

bool FMinderaSocket::Close()
{
	if (Transport)
	{
		Transport->CloseChannel(Channel);
	}
	return true;
}

bool FMinderaSocket::Bind(const FInternetAddr& Addr)
{
	// The bind address carries our local peer + the channel we listen on.
	BoundAddr = static_cast<const FInternetAddrMindera&>(Addr);
	Channel = Addr.GetPort();
	return Transport && Transport->OpenChannel(Channel);
}

bool FMinderaSocket::Connect(const FInternetAddr& Addr)
{
	// Connectionless: just record the peer we'll Send() to and make sure our channel is live.
	PeerAddr = static_cast<const FInternetAddrMindera&>(Addr);
	Channel = Addr.GetPort();
	return Transport && Transport->OpenChannel(Channel);
}

bool FMinderaSocket::Listen(int32 MaxBacklog)
{
	return Transport && Transport->OpenChannel(Channel);
}

bool FMinderaSocket::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	BytesSent = 0;
	if (!Transport)
	{
		return false;
	}
	const FInternetAddrMindera& Dest = static_cast<const FInternetAddrMindera&>(Destination);
	const int32 DestChannel = Destination.GetPort() != 0 ? Destination.GetPort() : Channel;
	if (Transport->SendTo(Dest.GetPeerId(), DestChannel, Data, Count, /*bReliable*/ true))
	{
		BytesSent = Count;
		return true;
	}
	return false;
}

bool FMinderaSocket::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	return SendTo(Data, Count, BytesSent, PeerAddr);
}

bool FMinderaSocket::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	BytesRead = 0;
	if (!Transport)
	{
		return false;
	}

	FString PeerId;
	TArray<uint8> Packet;
	if (!Transport->ReceiveFrom(Channel, PeerId, Packet))
	{
		return false;
	}

	BytesRead = FMath::Min(BufferSize, Packet.Num());
	FMemory::Memcpy(Data, Packet.GetData(), BytesRead);

	FInternetAddrMindera& Src = static_cast<FInternetAddrMindera&>(Source);
	Src.SetPeerId(PeerId);
	Src.SetPort(Channel);
	return BytesRead > 0;
}

bool FMinderaSocket::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	FInternetAddrMindera Source;
	return RecvFrom(Data, BufferSize, BytesRead, Source, Flags);
}

bool FMinderaSocket::HasPendingData(uint32& PendingDataSize)
{
	PendingDataSize = 0;
	return false;
}

ESocketConnectionState FMinderaSocket::GetConnectionState()
{
	return (Transport && Transport->IsAvailable()) ? SCS_Connected : SCS_NotConnected;
}

void FMinderaSocket::GetAddress(FInternetAddr& OutAddr)
{
	FInternetAddrMindera& Out = static_cast<FInternetAddrMindera&>(OutAddr);
	Out.SetPeerId(Transport ? Transport->GetLocalPeerId() : FString());
	Out.SetPort(Channel);
}

bool FMinderaSocket::GetPeerAddress(FInternetAddr& OutAddr)
{
	static_cast<FInternetAddrMindera&>(OutAddr) = PeerAddr;
	return !PeerAddr.GetPeerId().IsEmpty();
}

// Copyright Mindera. All Rights Reserved.

#include "NetDriver/MinderaSocket.h"
#include "NetDriver/MinderaSocketSubsystem.h"
#include "Native/Interfaces/IP2PTransport.h"

// SDK-free: everything routes through IP2PTransport.
//
// The transport is connectionless and identifies peers by id, so a single fixed channel carries all
// traffic — UE addresses/ports are bookkeeping only and never reach the SDK. Using one channel avoids
// any client/server channel-matching (a UE connect URL has no explicit P2P channel).
namespace
{
	constexpr int32 GMinderaP2PChannel = 0;
}

IP2PTransport* FMinderaSocket::GetTransport()
{
	// Live lookup (null-safe): the module returns nullptr once the platform is torn down, so a socket
	// destroyed by GC after teardown becomes a safe no-op instead of dereferencing a freed transport.
	return FMinderaSocketSubsystem::GetTransport();
}

FMinderaSocket::FMinderaSocket(ESocketType InSocketType, const FString& InSocketDescription, const FName& InSocketProtocol)
	: FSocket(InSocketType, InSocketDescription, InSocketProtocol)
{
}

FMinderaSocket::~FMinderaSocket()
{
	Close();
}

bool FMinderaSocket::Close()
{
	if (IP2PTransport* Transport = GetTransport())
	{
		Transport->CloseChannel(Channel);
	}
	return true;
}

bool FMinderaSocket::Bind(const FInternetAddr& Addr)
{
	// Record the local peer for bookkeeping; all traffic rides the single fixed channel.
	BoundAddr = static_cast<const FInternetAddrMindera&>(Addr);
	Channel = GMinderaP2PChannel;
	IP2PTransport* Transport = GetTransport();
	return Transport && Transport->OpenChannel(GMinderaP2PChannel);
}

bool FMinderaSocket::Connect(const FInternetAddr& Addr)
{
	// Connectionless: just record the peer we'll Send() to and make sure our channel is live.
	PeerAddr = static_cast<const FInternetAddrMindera&>(Addr);
	Channel = GMinderaP2PChannel;
	IP2PTransport* Transport = GetTransport();
	return Transport && Transport->OpenChannel(GMinderaP2PChannel);
}

bool FMinderaSocket::Listen(int32 MaxBacklog)
{
	IP2PTransport* Transport = GetTransport();
	return Transport && Transport->OpenChannel(GMinderaP2PChannel);
}

bool FMinderaSocket::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	BytesSent = 0;
	IP2PTransport* Transport = GetTransport();
	if (!Transport)
	{
		return false;
	}
	const FInternetAddrMindera& Dest = static_cast<const FInternetAddrMindera&>(Destination);
	if (Transport->SendTo(Dest.GetPeerId(), GMinderaP2PChannel, Data, Count, /*bReliable*/ true))
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
	IP2PTransport* Transport = GetTransport();
	if (!Transport)
	{
		return false;
	}

	FString PeerId;
	TArray<uint8> Packet;
	if (!Transport->ReceiveFrom(GMinderaP2PChannel, PeerId, Packet))
	{
		return false;
	}

	BytesRead = FMath::Min(BufferSize, Packet.Num());
	FMemory::Memcpy(Data, Packet.GetData(), BytesRead);

	// Source is identified purely by peer id; the port is fixed so connections match by id alone.
	FInternetAddrMindera& Src = static_cast<FInternetAddrMindera&>(Source);
	Src.SetPeerId(PeerId);
	Src.SetPort(GMinderaP2PChannel);
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
	IP2PTransport* Transport = GetTransport();
	return (Transport && Transport->IsAvailable()) ? SCS_Connected : SCS_NotConnected;
}

void FMinderaSocket::GetAddress(FInternetAddr& OutAddr)
{
	IP2PTransport* Transport = GetTransport();
	FInternetAddrMindera& Out = static_cast<FInternetAddrMindera&>(OutAddr);
	Out.SetPeerId(Transport ? Transport->GetLocalPeerId() : FString());
	Out.SetPort(Channel);
}

bool FMinderaSocket::GetPeerAddress(FInternetAddr& OutAddr)
{
	static_cast<FInternetAddrMindera&>(OutAddr) = PeerAddr;
	return !PeerAddr.GetPeerId().IsEmpty();
}

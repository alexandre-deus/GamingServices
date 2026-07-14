// Copyright Mindera. All Rights Reserved.

#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSP2PTransport.h"

#include "eos_p2p.h"
#include "eos_common.h"

#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogEOSP2P, Log, All);

namespace GamingServices
{
	// One socket name for all game traffic; the netdriver's virtual port maps to the EOS channel.
	static const char* GEOSSocketName = "MinderaP2P";

	static EOS_P2P_SocketId MakeSocketId()
	{
		EOS_P2P_SocketId SocketId = {};
		SocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		FCStringAnsi::Strncpy(SocketId.SocketName, GEOSSocketName, EOS_P2P_SOCKETID_SOCKETNAME_SIZE);
		return SocketId;
	}

	static FString PuidToString(EOS_ProductUserId Puid)
	{
		if (!Puid)
		{
			return FString();
		}
		char Buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
		int32_t Len = sizeof(Buffer);
		if (EOS_ProductUserId_ToString(Puid, Buffer, &Len) == EOS_EResult::EOS_Success)
		{
			return UTF8_TO_TCHAR(Buffer);
		}
		return FString();
	}

	struct FEOSP2PTransport::FImpl
	{
		EOS_HP2P P2PHandle = nullptr;
		EOS_ProductUserId LocalUser = nullptr;

		EOS_NotificationId ConnectionRequestId = EOS_INVALID_NOTIFICATIONID;
		EOS_NotificationId ConnectionClosedId = EOS_INVALID_NOTIFICATIONID;

		TSet<int32> OpenChannels;
		TArray<FString> ClosedPeers;
	};

	FEOSP2PTransport::FEOSP2PTransport(void* InP2PHandle, void* InLocalUser)
	{
		Impl = MakePimpl<FImpl>();
		Impl->P2PHandle = static_cast<EOS_HP2P>(InP2PHandle);
		Impl->LocalUser = static_cast<EOS_ProductUserId>(InLocalUser);

		if (!Impl->P2PHandle || !Impl->LocalUser)
		{
			UE_LOG(LogEOSP2P, Warning, TEXT("FEOSP2PTransport: constructed without a valid P2P handle / local user"));
			return;
		}

		// Auto-accept every incoming connection request on our socket so packets can flow both ways.
		EOS_P2P_AddNotifyPeerConnectionRequestOptions RequestOptions = {};
		RequestOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
		RequestOptions.LocalUserId = Impl->LocalUser;
		EOS_P2P_SocketId SocketId = MakeSocketId();
		RequestOptions.SocketId = &SocketId;

		Impl->ConnectionRequestId = EOS_P2P_AddNotifyPeerConnectionRequest(
			Impl->P2PHandle, &RequestOptions, this,
			[](const EOS_P2P_OnIncomingConnectionRequestInfo* Data)
			{
				check(Data);
				auto* Self = static_cast<FEOSP2PTransport*>(Data->ClientData);
				if (!Self || !Self->Impl->P2PHandle)
				{
					return;
				}
				EOS_P2P_AcceptConnectionOptions AcceptOptions = {};
				AcceptOptions.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
				AcceptOptions.LocalUserId = Data->LocalUserId;
				AcceptOptions.RemoteUserId = Data->RemoteUserId;
				AcceptOptions.SocketId = Data->SocketId;
				const EOS_EResult Result = EOS_P2P_AcceptConnection(Self->Impl->P2PHandle, &AcceptOptions);
				UE_LOG(LogEOSP2P, Log, TEXT("FEOSP2PTransport: accepted incoming connection from %s (result %d)"),
				       *PuidToString(Data->RemoteUserId), (int32)Result);
			});

		EOS_P2P_AddNotifyPeerConnectionClosedOptions ClosedOptions = {};
		ClosedOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONCLOSED_API_LATEST;
		ClosedOptions.LocalUserId = Impl->LocalUser;
		ClosedOptions.SocketId = &SocketId;

		Impl->ConnectionClosedId = EOS_P2P_AddNotifyPeerConnectionClosed(
			Impl->P2PHandle, &ClosedOptions, this,
			[](const EOS_P2P_OnRemoteConnectionClosedInfo* Data)
			{
				check(Data);
				auto* Self = static_cast<FEOSP2PTransport*>(Data->ClientData);
				if (!Self)
				{
					return;
				}
				const FString Peer = PuidToString(Data->RemoteUserId);
				UE_LOG(LogEOSP2P, Log, TEXT("FEOSP2PTransport: connection closed with %s (reason %d)"),
				       *Peer, (int32)Data->Reason);
				if (!Peer.IsEmpty())
				{
					Self->Impl->ClosedPeers.Add(Peer);
				}
			});
	}

	FEOSP2PTransport::~FEOSP2PTransport()
	{
		if (!Impl || !Impl->P2PHandle)
		{
			return;
		}
		if (Impl->ConnectionRequestId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_P2P_RemoveNotifyPeerConnectionRequest(Impl->P2PHandle, Impl->ConnectionRequestId);
		}
		if (Impl->ConnectionClosedId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_P2P_RemoveNotifyPeerConnectionClosed(Impl->P2PHandle, Impl->ConnectionClosedId);
		}

		// Tear down any open sessions on our socket.
		EOS_P2P_SocketId SocketId = MakeSocketId();
		EOS_P2P_CloseConnectionsOptions CloseOptions = {};
		CloseOptions.ApiVersion = EOS_P2P_CLOSECONNECTIONS_API_LATEST;
		CloseOptions.LocalUserId = Impl->LocalUser;
		CloseOptions.SocketId = &SocketId;
		EOS_P2P_CloseConnections(Impl->P2PHandle, &CloseOptions);
	}

	bool FEOSP2PTransport::IsAvailable() const
	{
		return Impl && Impl->P2PHandle != nullptr && Impl->LocalUser != nullptr;
	}

	FString FEOSP2PTransport::GetLocalPeerId() const
	{
		return Impl ? PuidToString(Impl->LocalUser) : FString();
	}

	bool FEOSP2PTransport::OpenChannel(int32 Channel)
	{
		if (!IsAvailable())
		{
			return false;
		}
		Impl->OpenChannels.Add(Channel);
		return true;
	}

	void FEOSP2PTransport::CloseChannel(int32 Channel)
	{
		if (Impl)
		{
			Impl->OpenChannels.Remove(Channel);
		}
	}

	bool FEOSP2PTransport::SendTo(const FString& RemotePeerId, int32 Channel, const void* Data, int32 CountBytes, bool bReliable)
	{
		if (!IsAvailable() || CountBytes <= 0 || CountBytes > EOS_P2P_MAX_PACKET_SIZE)
		{
			return false;
		}

		const EOS_ProductUserId Remote = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*RemotePeerId));
		if (!EOS_ProductUserId_IsValid(Remote))
		{
			UE_LOG(LogEOSP2P, Warning, TEXT("FEOSP2PTransport: SendTo invalid peer id '%s'"), *RemotePeerId);
			return false;
		}

		EOS_P2P_SocketId SocketId = MakeSocketId();
		EOS_P2P_SendPacketOptions Options = {};
		Options.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
		Options.LocalUserId = Impl->LocalUser;
		Options.RemoteUserId = Remote;
		Options.SocketId = &SocketId;
		Options.Channel = static_cast<uint8_t>(Channel);
		Options.DataLengthBytes = static_cast<uint32_t>(CountBytes);
		Options.Data = Data;
		Options.bAllowDelayedDelivery = EOS_TRUE;
		Options.Reliability = bReliable
			                      ? EOS_EPacketReliability::EOS_PR_ReliableOrdered
			                      : EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
		Options.bDisableAutoAcceptConnection = EOS_FALSE;

		const EOS_EResult Result = EOS_P2P_SendPacket(Impl->P2PHandle, &Options);
		if (Result != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogEOSP2P, Warning, TEXT("FEOSP2PTransport: SendPacket to %s failed: %d"), *RemotePeerId, (int32)Result);
			return false;
		}
		return true;
	}

	bool FEOSP2PTransport::ReceiveFrom(int32 Channel, FString& OutRemotePeerId, TArray<uint8>& OutData)
	{
		if (!IsAvailable())
		{
			return false;
		}

		const uint8_t ChannelByte = static_cast<uint8_t>(Channel);

		EOS_P2P_GetNextReceivedPacketSizeOptions SizeOptions = {};
		SizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
		SizeOptions.LocalUserId = Impl->LocalUser;
		SizeOptions.RequestedChannel = &ChannelByte;

		uint32_t PacketSize = 0;
		if (EOS_P2P_GetNextReceivedPacketSize(Impl->P2PHandle, &SizeOptions, &PacketSize) != EOS_EResult::EOS_Success
			|| PacketSize == 0)
		{
			return false;
		}

		OutData.SetNumUninitialized(static_cast<int32>(PacketSize));

		EOS_ProductUserId PeerId = nullptr;
		EOS_P2P_SocketId SocketId = {};
		uint8_t OutChannel = 0;
		uint32_t BytesWritten = 0;

		EOS_P2P_ReceivePacketOptions ReceiveOptions = {};
		ReceiveOptions.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
		ReceiveOptions.LocalUserId = Impl->LocalUser;
		ReceiveOptions.MaxDataSizeBytes = PacketSize;
		ReceiveOptions.RequestedChannel = &ChannelByte;

		const EOS_EResult Result = EOS_P2P_ReceivePacket(
			Impl->P2PHandle, &ReceiveOptions, &PeerId, &SocketId, &OutChannel, OutData.GetData(), &BytesWritten);
		if (Result != EOS_EResult::EOS_Success)
		{
			OutData.Reset();
			return false;
		}

		OutData.SetNum(static_cast<int32>(BytesWritten), EAllowShrinking::No);
		OutRemotePeerId = PuidToString(PeerId);
		return true;
	}

	void FEOSP2PTransport::Tick()
	{
		// EOS notifications are pumped by EOS_Platform_Tick (driven by the owning subsystem), so there
		// is nothing per-transport to pump here.
	}

	void FEOSP2PTransport::PumpClosedPeers(TArray<FString>& OutClosedPeerIds)
	{
		if (Impl)
		{
			OutClosedPeerIds.Append(Impl->ClosedPeers);
			Impl->ClosedPeers.Reset();
		}
	}
}

#endif // USE_EOS

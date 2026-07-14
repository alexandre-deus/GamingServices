// Copyright Mindera. All Rights Reserved.

#ifdef USE_STEAMWORKS

#include "Native/Steam/Interfaces/SteamP2PTransport.h"

#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingtypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSteamP2P, Log, All);

namespace GamingServices
{
	static ISteamNetworkingSockets* GetSteamSockets()
	{
		if (IsRunningDedicatedServer() && SteamGameServerNetworkingSockets())
		{
			return SteamGameServerNetworkingSockets();
		}
		return SteamNetworkingSockets();
	}

	static uint64 LocalSteamId64()
	{
		if (IsRunningDedicatedServer() && SteamGameServer())
		{
			return SteamGameServer()->GetSteamID().ConvertToUint64();
		}
		return SteamUser() ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
	}

	struct FSteamP2PTransport::FImpl
	{
		HSteamListenSocket ListenSocket = k_HSteamListenSocket_Invalid;
		HSteamNetPollGroup PollGroup = k_HSteamNetPollGroup_Invalid;
		int32 VirtualPort = 0;
		bool bChannelOpen = false;

		// The bridge: peer SteamID64 -> its (incoming or outgoing) connection handle.
		TMap<uint64, HSteamNetConnection> PeerToConn;
		TArray<FString> ClosedPeers;
	};

	// One transport per process; the Steam status callback is a C function pointer, so it routes here.
	static FSteamP2PTransport* GActiveTransport = nullptr;

	static void ConnectionStatusChangedThunk(SteamNetConnectionStatusChangedCallback_t* Info);

	FSteamP2PTransport::FSteamP2PTransport()
	{
		Impl = MakePimpl<FImpl>();
		GActiveTransport = this;
	}

	FSteamP2PTransport::~FSteamP2PTransport()
	{
		if (ISteamNetworkingSockets* Sockets = GetSteamSockets())
		{
			for (const TPair<uint64, HSteamNetConnection>& Pair : Impl->PeerToConn)
			{
				Sockets->CloseConnection(Pair.Value, k_ESteamNetConnectionEnd_App_Generic, "Transport shutdown", false);
			}
			if (Impl->PollGroup != k_HSteamNetPollGroup_Invalid)
			{
				Sockets->DestroyPollGroup(Impl->PollGroup);
			}
			if (Impl->ListenSocket != k_HSteamListenSocket_Invalid)
			{
				Sockets->CloseListenSocket(Impl->ListenSocket);
			}
		}
		Impl->PeerToConn.Empty();
		if (GActiveTransport == this)
		{
			GActiveTransport = nullptr;
		}
	}

	bool FSteamP2PTransport::IsAvailable() const
	{
		return GetSteamSockets() != nullptr;
	}

	FString FSteamP2PTransport::GetLocalPeerId() const
	{
		return FString::Printf(TEXT("%llu"), LocalSteamId64());
	}

	bool FSteamP2PTransport::OpenChannel(int32 Channel)
	{
		ISteamNetworkingSockets* Sockets = GetSteamSockets();
		if (!Sockets)
		{
			return false;
		}
		if (Impl->bChannelOpen)
		{
			return true;
		}

		Impl->VirtualPort = Channel;
		SteamNetworkingUtils()->InitRelayNetworkAccess();

		// Route this socket's connection-status changes to our thunk.
		SteamNetworkingConfigValue_t Opt;
		Opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)&ConnectionStatusChangedThunk);

		Impl->ListenSocket = Sockets->CreateListenSocketP2P(Channel, 1, &Opt);
		if (Impl->ListenSocket == k_HSteamListenSocket_Invalid)
		{
			UE_LOG(LogSteamP2P, Error, TEXT("FSteamP2PTransport: CreateListenSocketP2P failed (vport %d)"), Channel);
			return false;
		}

		Impl->PollGroup = Sockets->CreatePollGroup();
		if (Impl->PollGroup == k_HSteamNetPollGroup_Invalid)
		{
			Sockets->CloseListenSocket(Impl->ListenSocket);
			Impl->ListenSocket = k_HSteamListenSocket_Invalid;
			UE_LOG(LogSteamP2P, Error, TEXT("FSteamP2PTransport: CreatePollGroup failed"));
			return false;
		}

		Impl->bChannelOpen = true;
		return true;
	}

	void FSteamP2PTransport::CloseChannel(int32 Channel)
	{
		// Channels share one listen socket + poll group; teardown happens in the destructor.
	}

	bool FSteamP2PTransport::SendTo(const FString& RemotePeerId, int32 Channel, const void* Data, int32 CountBytes, bool bReliable)
	{
		ISteamNetworkingSockets* Sockets = GetSteamSockets();
		if (!Sockets || CountBytes <= 0)
		{
			return false;
		}

		const uint64 PeerId = FCString::Atoi64(*RemotePeerId);
		if (PeerId == 0)
		{
			return false;
		}

		HSteamNetConnection Conn = k_HSteamNetConnection_Invalid;
		if (const HSteamNetConnection* Existing = Impl->PeerToConn.Find(PeerId))
		{
			Conn = *Existing;
		}
		else
		{
			// First packet to this peer: open an outgoing connection and join it to the poll group so
			// ReceiveFrom drains it alongside accepted connections.
			SteamNetworkingUtils()->InitRelayNetworkAccess();
			SteamNetworkingConfigValue_t Opt;
			Opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)&ConnectionStatusChangedThunk);

			SteamNetworkingIdentity Identity;
			Identity.SetSteamID64(PeerId);
			Conn = Sockets->ConnectP2P(Identity, Channel, 1, &Opt);
			if (Conn == k_HSteamNetConnection_Invalid)
			{
				UE_LOG(LogSteamP2P, Warning, TEXT("FSteamP2PTransport: ConnectP2P to %llu failed"), PeerId);
				return false;
			}
			if (Impl->PollGroup != k_HSteamNetPollGroup_Invalid)
			{
				Sockets->SetConnectionPollGroup(Conn, Impl->PollGroup);
			}
			Impl->PeerToConn.Add(PeerId, Conn);
		}

		const int32 SendFlags = bReliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_UnreliableNoNagle;
		const EResult Result = Sockets->SendMessageToConnection(
			Conn, Data, static_cast<uint32>(CountBytes), SendFlags, nullptr);
		return Result == k_EResultOK;
	}

	bool FSteamP2PTransport::ReceiveFrom(int32 Channel, FString& OutRemotePeerId, TArray<uint8>& OutData)
	{
		ISteamNetworkingSockets* Sockets = GetSteamSockets();
		if (!Sockets || Impl->PollGroup == k_HSteamNetPollGroup_Invalid)
		{
			return false;
		}

		SteamNetworkingMessage_t* Message = nullptr;
		const int32 Count = Sockets->ReceiveMessagesOnPollGroup(Impl->PollGroup, &Message, 1);
		if (Count <= 0 || !Message)
		{
			return false;
		}

		OutRemotePeerId = FString::Printf(TEXT("%llu"), Message->m_identityPeer.GetSteamID64());
		OutData.SetNumUninitialized(Message->m_cbSize);
		FMemory::Memcpy(OutData.GetData(), Message->m_pData, Message->m_cbSize);
		Message->Release();
		return true;
	}

	void FSteamP2PTransport::Tick()
	{
		// Dispatches the connection-status callbacks registered on our sockets.
		if (ISteamNetworkingSockets* Sockets = GetSteamSockets())
		{
			Sockets->RunCallbacks();
		}
	}

	void FSteamP2PTransport::PumpClosedPeers(TArray<FString>& OutClosedPeerIds)
	{
		OutClosedPeerIds.Append(Impl->ClosedPeers);
		Impl->ClosedPeers.Reset();
	}

	// ---- Connection-status callback ----------------------------------------------------------------
	void FSteamP2PTransport::HandleConnectionStatus(void* CallbackInfo)
	{
		auto* Info = static_cast<SteamNetConnectionStatusChangedCallback_t*>(CallbackInfo);
		ISteamNetworkingSockets* Sockets = GetSteamSockets();
		if (!Sockets || !Info)
		{
			return;
		}
		const HSteamNetConnection Conn = Info->m_hConn;
		const uint64 PeerId = Info->m_info.m_identityRemote.GetSteamID64();

		switch (Info->m_info.m_eState)
		{
		case k_ESteamNetworkingConnectionState_Connecting:
			// Incoming connection on our listen socket — accept it, poll-group it, map the peer.
			if (Info->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid)
			{
				if (Sockets->AcceptConnection(Conn) == k_EResultOK)
				{
					if (Impl->PollGroup != k_HSteamNetPollGroup_Invalid)
					{
						Sockets->SetConnectionPollGroup(Conn, Impl->PollGroup);
					}
					Impl->PeerToConn.Add(PeerId, Conn);
					UE_LOG(LogSteamP2P, Log, TEXT("FSteamP2PTransport: accepted incoming from %llu (conn=%u)"), PeerId, Conn);
				}
				else
				{
					Sockets->CloseConnection(Conn, 0, "AcceptConnection failed", false);
				}
			}
			break;

		case k_ESteamNetworkingConnectionState_Connected:
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			Sockets->CloseConnection(Conn, 0, nullptr, false);
			Impl->PeerToConn.Remove(PeerId);
			Impl->ClosedPeers.Add(FString::Printf(TEXT("%llu"), PeerId));
			UE_LOG(LogSteamP2P, Log, TEXT("FSteamP2PTransport: connection closed with %llu (conn=%u)"), PeerId, Conn);
			break;

		case k_ESteamNetworkingConnectionState_None:
			Sockets->CloseConnection(Conn, 0, nullptr, false);
			Impl->PeerToConn.Remove(PeerId);
			break;

		default:
			break;
		}
	}

	static void ConnectionStatusChangedThunk(SteamNetConnectionStatusChangedCallback_t* Info)
	{
		if (GActiveTransport && Info)
		{
			GActiveTransport->HandleConnectionStatus(Info);
		}
	}
}

#endif // USE_STEAMWORKS

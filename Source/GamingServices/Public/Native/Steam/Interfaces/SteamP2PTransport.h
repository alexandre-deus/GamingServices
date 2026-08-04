// Copyright Mindera. All Rights Reserved.

#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IP2PTransport.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	/**
	 * Steam implementation of the connectionless transport. ISteamNetworkingSockets is connection-
	 * oriented, so this class keeps the bridge INTERNALLY: a peer<->connection map, auto-connect on
	 * first SendTo to a peer, auto-accept of incoming connections (via the status callback), and a
	 * single poll group that all connections join so ReceiveFrom can drain everything at once. That is
	 * exactly the machinery that used to live in the driver; here it stays a Steam-only detail so the
	 * netdriver base classes see only the flat SendTo/ReceiveFrom interface.
	 *
	 * SDK-free header: all Steam types live in the .cpp's FImpl.
	 */
	class FSteamP2PTransport final : public IP2PTransport
	{
	public:
		FSteamP2PTransport();
		virtual ~FSteamP2PTransport() override;

		virtual bool IsAvailable() const override;
		virtual FString GetLocalPeerId() const override;
		virtual FString GetUrlPrefix() const override { return TEXT("steam."); }

		virtual bool OpenChannel(int32 Channel) override;
		virtual void CloseChannel(int32 Channel) override;

		virtual bool SendTo(const FString& RemotePeerId, int32 Channel, const void* Data, int32 CountBytes, bool bReliable) override;
		virtual bool ReceiveFrom(int32 Channel, FString& OutRemotePeerId, TArray<uint8>& OutData) override;

		virtual void Tick() override;
		virtual void PumpClosedPeers(TArray<FString>& OutClosedPeerIds) override;

		/**
		 * Routes a Steam SteamNetConnectionStatusChangedCallback_t* (passed as void* to keep the SDK out
		 * of this header) into the pimpl. Called by the C status-callback thunk in the .cpp.
		 */
		void HandleConnectionStatus(void* CallbackInfo);

	private:
		struct FImpl;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_STEAM

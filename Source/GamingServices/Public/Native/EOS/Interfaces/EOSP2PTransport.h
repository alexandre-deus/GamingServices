// Copyright Mindera. All Rights Reserved.

#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IP2PTransport.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	/**
	 * EOS P2P implementation of the connectionless transport. Natively packet-based: SendTo maps to
	 * EOS_P2P_SendPacket and ReceiveFrom to EOS_P2P_ReceivePacket, so no peer<->connection bridging is
	 * needed. Incoming connections are auto-accepted from the connection-request notification, and
	 * connection-closed notifications feed PumpClosedPeers.
	 *
	 * SDK-free header: the EOS_HP2P handle and local EOS_ProductUserId are passed in as void* and all
	 * EOS types live in the .cpp's FImpl.
	 */
	class FEOSP2PTransport final : public IP2PTransport
	{
	public:
		/** InP2PHandle is an EOS_HP2P; InLocalUser is the local EOS_ProductUserId. */
		FEOSP2PTransport(void* InP2PHandle, void* InLocalUser);
		virtual ~FEOSP2PTransport() override;

		virtual bool IsAvailable() const override;
		virtual FString GetLocalPeerId() const override;
		virtual FString GetUrlPrefix() const override { return TEXT("eos."); }

		virtual bool OpenChannel(int32 Channel) override;
		virtual void CloseChannel(int32 Channel) override;

		virtual bool SendTo(const FString& RemotePeerId, int32 Channel, const void* Data, int32 CountBytes, bool bReliable) override;
		virtual bool ReceiveFrom(int32 Channel, FString& OutRemotePeerId, TArray<uint8>& OutData) override;

		virtual void Tick() override;
		virtual void PumpClosedPeers(TArray<FString>& OutClosedPeerIds) override;

	private:
		struct FImpl;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // USE_EOS

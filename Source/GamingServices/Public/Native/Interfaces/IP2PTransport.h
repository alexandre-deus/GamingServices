// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * SDK-free, CONNECTIONLESS abstraction over a platform P2P networking stack (Steam / EOS).
 *
 * This is the P2P capability's common interface, alongside the other per-backend capabilities
 * (IUserService, ILeaderboardsService, ...). It is consumed by the generic Mindera netdriver base
 * classes, but it belongs to each backend just like the rest: the common interface lives here and
 * each backend supplies its implementation under Native/<Backend>/Interfaces/.
 *
 * UE's netdriver model is connectionless: UIpNetDriver rides a datagram socket and dispatches by
 * address, tracking its UNetConnections in a map. This interface matches that model directly — you
 * send a packet to a peer id and receive packets tagged with their source peer id, on a virtual-port
 * "channel". No connection handles, poll groups or accept dance appear here.
 *
 * The generic Mindera netdriver base classes (FInternetAddrMindera / FMinderaSocket /
 * FMinderaSocketSubsystem / UMinderaNetDriver / UMinderaNetConnection) talk ONLY to this interface —
 * no platform SDK type ever appears in a netdriver header. Each backend implements it in a single
 * .cpp that includes just its own SDK:
 *   - SteamP2PTransport.cpp -> steam/*.h : ISteamNetworkingSockets is connection-oriented, so this
 *       impl keeps the peer<->connection map + auto-connect/auto-accept INTERNALLY (the bridge that
 *       used to live in the driver). That preserves the proven relay behaviour.
 *   - EOSP2PTransport.cpp   -> eos_p2p.h : natively connectionless — SendTo == EOS_P2P_SendPacket,
 *       ReceiveFrom == EOS_P2P_ReceivePacket. No bridge needed.
 *
 * Peers are addressed by an opaque id STRING (a SteamID64 or an EOS ProductUserId in text form).
 */
class GAMINGSERVICES_API IP2PTransport
{
public:
	virtual ~IP2PTransport() = default;

	/** Whether the platform networking stack is up (relay / login ready). */
	virtual bool IsAvailable() const = 0;

	/** Local user's peer id in text form, used to build the listen address a client connects to. */
	virtual FString GetLocalPeerId() const = 0;

	/**
	 * URL host prefix that tags this backend's peer ids ("steam." / "eos."). The SDK-agnostic netdriver
	 * asks the active transport for it rather than hard-coding per-backend constants: it uses the prefix
	 * to recognise a P2P connect URL and to strip it back down to the raw peer id.
	 */
	virtual FString GetUrlPrefix() const = 0;

	/**
	 * Begin sending/receiving on a virtual port (channel). Servers call this to start accepting
	 * incoming packets; clients call it before their first SendTo. Idempotent per channel.
	 */
	virtual bool OpenChannel(int32 Channel) = 0;
	virtual void CloseChannel(int32 Channel) = 0;

	/** Send a datagram to a peer on a channel. bReliable selects reliable vs unreliable delivery. */
	virtual bool SendTo(const FString& RemotePeerId, int32 Channel, const void* Data, int32 CountBytes, bool bReliable) = 0;

	/**
	 * Pop the next received datagram on a channel. Returns false when none is queued; otherwise fills
	 * the source peer id and the payload.
	 */
	virtual bool ReceiveFrom(int32 Channel, FString& OutRemotePeerId, TArray<uint8>& OutData) = 0;

	/** Pump platform callbacks: accept incoming sessions/connections, drain relay events, etc. */
	virtual void Tick() = 0;

	/**
	 * Append peers whose connection has dropped since the last call, so the netdriver can close the
	 * matching UNetConnection. (Steam surfaces this from its status callback; EOS from its
	 * peer-connection-closed notification.)
	 */
	virtual void PumpClosedPeers(TArray<FString>& OutClosedPeerIds) = 0;
};

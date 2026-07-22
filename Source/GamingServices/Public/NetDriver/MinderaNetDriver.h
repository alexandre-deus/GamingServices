// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IpNetDriver.h"
#include "NetDriver/MinderaInternetAddr.h"
#include "MinderaNetDriver.generated.h"

class IP2PTransport;

/**
 * P2P net driver over an IP2PTransport (Steam / EOS). Modelled on the engine's USteamNetDriver: a thin
 * UIpNetDriver that only swaps in the Mindera P2P socket subsystem and lets the base class do all the
 * real work — it creates/binds the FSocket through our subsystem, builds the UNetConnections and runs
 * the connectionless handshake exactly as it would for UDP.
 *
 * Its only P2P-specific behaviour is:
 *   - choosing the Mindera socket subsystem vs the platform one (GetSocketSubsystem / passthrough),
 *   - a non-zero client bind "port" so the address resolver's bind sockets succeed (the transport
 *     itself is connectionless and ignores ports — see FMinderaSocket),
 *   - closing UNetConnections when the transport reports a peer dropped (P2P has no OS-level reset).
 *
 * Passthrough (plain IP) is chosen purely by URL: non-P2P hosts (LAN / PIE / raw IP) fall back to the
 * platform socket subsystem, so no GIsEditor special-casing is needed.
 *
 * SDK-free header: no platform networking type appears here.
 */
UCLASS(Transient, Config = Engine)
class GAMINGSERVICES_API UMinderaNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:
	UMinderaNetDriver(const FObjectInitializer& ObjectInitializer);

	// ~UNetDriver / UIpNetDriver
	virtual bool IsAvailable() const override;
	virtual ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual int GetClientPort() override;
	virtual void TickDispatch(float DeltaTime) override;
	// ~UNetDriver / UIpNetDriver

	/** True when this driver falls back to the plain IP path (LAN / PIE / non-P2P URL). */
	bool bIsPassthrough = false;

private:
	IP2PTransport* GetTransport() const;

	/** Whether a URL host targets P2P (starts with the active transport's "steam."/"eos." prefix). */
	bool IsP2PUrl(const FString& Host) const;

	/** Close the UNetConnection whose remote address is PeerId (called when the transport drops a peer). */
	void CloseConnectionForPeer(const FString& PeerId);
};

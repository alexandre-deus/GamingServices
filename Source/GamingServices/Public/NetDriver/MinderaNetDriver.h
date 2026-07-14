// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IpNetDriver.h"
#include "NetDriver/MinderaInternetAddr.h"
#include "MinderaNetDriver.generated.h"

class FMinderaSocket;
class IP2PTransport;

/**
 * P2P net driver over an IP2PTransport (Steam / EOS). Because the transport is a connectionless
 * datagram, this is a thin UIpNetDriver: it creates an FMinderaSocket through the Mindera socket
 * subsystem and otherwise relies on UIpNetDriver's address-based dispatch and connectionless
 * handshake. Its only P2P-specific work is (a) a plain-IP passthrough path for PIE / non-P2P URLs and
 * (b) closing UNetConnections when the transport reports a peer dropped (P2P has no OS-level reset).
 *
 * SDK-free header: no platform networking type appears here.
 */
UCLASS(Transient, Config = Engine)
class GAMINGSERVICES_API UMinderaNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:
	UMinderaNetDriver(const FObjectInitializer& ObjectInitializer);

	// ~UNetDriver
	virtual bool IsAvailable() const override;
	virtual ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void Shutdown() override;
	virtual bool IsNetResourceValid() override;
	// ~UNetDriver

	/** True when this driver falls back to the plain IP path (PIE / non-P2P URL). */
	bool bIsPassthrough = false;

	/** Virtual port / channel used for P2P listen & connect. */
	UPROPERTY(Config)
	int32 P2PVirtualPort = 0;

	/** The datagram socket created by this driver (client or listen). */
	TSharedPtr<FMinderaSocket> P2PSocket;

private:
	IP2PTransport* GetTransport() const;

	/** Whether a URL host targets P2P (starts with the active transport's "steam."/"eos." prefix). */
	bool IsP2PUrl(const FString& Host) const;

	/** Close the UNetConnection whose remote address is PeerId (called when the transport drops a peer). */
	void CloseConnectionForPeer(const FString& PeerId);
};

// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IpNetDriver.h"
#include "steam/steamnetworkingtypes.h"
#include "NetDriver/MinderaInternetAddr.h"
#include "MinderaNetDriver.generated.h"

struct SteamNetConnectionStatusChangedCallback_t;
class ISteamNetworkingSockets;
class FMinderaSocket;

/**
 * Net driver that uses Steam Networking Sockets for P2P relay-based
 * connections while still allowing a plain-IP fallback path.
 * Mirrors the official SteamSockets plugin architecture: the driver
 * creates FMinderaSocket instances through FMinderaSocketSubsystem.
 */
UCLASS(Transient, Config = Engine)
class GAMINGSERVICES_API UMinderaNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:
	UMinderaNetDriver(const FObjectInitializer& ObjectInitializer);

	// ~UNetDriver
	virtual void PostInitProperties() override;
	virtual bool IsAvailable() const override;
	virtual ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void Shutdown() override;
	virtual bool IsNetResourceValid() override;
	// ~UNetDriver

	/** True when the connection should use the platform socket subsystem instead of Steam. */
	bool bIsPassthrough = false;

	/** Virtual port used for Steam P2P listen / connect. */
	UPROPERTY(Config)
	int32 SteamVirtualPort = 0;
	
	/** The Steam socket created by this driver (listen or client). */
	TSharedPtr<FMinderaSocket> SteamSocket;

	/**
	 * Maps accepted Steam connection handles to their remote identity.
	 * Populated in the Connecting callback, consumed in InitRemoteConnection
	 * when the engine creates the UNetConnection after handshake completes.
	 */
	TMap<HSteamNetConnection, SteamNetworkingIdentity> PendingSteamConnections;

	/** Look up the Steam connection handle for a given remote identity. */
	HSteamNetConnection FindSteamHandleForIdentity(const SteamNetworkingIdentity& Identity) const;

private:
	/** Returns the correct ISteamNetworkingSockets interface for client or server. */
	ISteamNetworkingSockets* GetSteamSocketsInterface() const;

	/** Called by the Steam callback framework when any connection changes state. */
	void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);

	/** Static thunk that routes to the singleton driver instance. */
	static void SteamConnectionStatusChangedThunk(SteamNetConnectionStatusChangedCallback_t* pInfo);

	/** Weak pointer back to the singleton driver registered for callbacks. */
	static UMinderaNetDriver* sActiveCallbackDriver;
};
// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IpConnection.h"
#include "steam/steamnetworkingtypes.h"
#include "MinderaNetConnection.generated.h"

class ISteamNetworkingSockets;
class FMinderaSocket;

/**
 * Net connection that rides on top of a Steam Networking Sockets P2P
 * connection via an FMinderaSocket. When the driver is in passthrough
 * mode the connection behaves like a regular UIpConnection.
 */
UCLASS(Transient, Config = Engine)
class GAMINGSERVICES_API UMinderaNetConnection : public UIpConnection
{
	GENERATED_BODY()

public:
	UMinderaNetConnection(const FObjectInitializer& ObjectInitializer);

	// ~UNetConnection
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void CleanUp() override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	// ~UNetConnection

	/** True when this connection should use plain IP instead of Steam relay. */
	bool bIsPassthrough = false;

	/** Steam Networking Sockets connection handle. k_HSteamNetConnection_Invalid when unused. */
	HSteamNetConnection SteamConnectionHandle = k_HSteamNetConnection_Invalid;

private:
	/** Returns the correct ISteamNetworkingSockets interface for the owning driver. */
	ISteamNetworkingSockets* GetSteamSocketsInterface() const;
};
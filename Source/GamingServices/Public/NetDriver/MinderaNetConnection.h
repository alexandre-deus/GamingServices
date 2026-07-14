// Copyright Mindera. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IpConnection.h"
#include "MinderaNetConnection.generated.h"

/**
 * Net connection for the Mindera P2P netdriver. Because the transport presents a connectionless
 * datagram socket, this is just a UIpConnection — packets are sent to / received from the peer
 * address through the driver's socket, exactly like the IP driver. SDK-free.
 */
UCLASS(Transient, Config = Engine)
class GAMINGSERVICES_API UMinderaNetConnection : public UIpConnection
{
	GENERATED_BODY()

public:
	UMinderaNetConnection(const FObjectInitializer& ObjectInitializer);

	/** True when this connection uses the plain IP passthrough path instead of P2P. */
	bool bIsPassthrough = false;
};

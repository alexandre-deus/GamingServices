#pragma once

#include "CoreMinimal.h"
#include "Engine/NetConnection.h"
#include "MinderaNetConnection.generated.h"

UCLASS(Transient)
class GAMINGSERVICES_API UMinderaNetConnection : public UNetConnection
{
	GENERATED_BODY()

public:
	virtual ~UMinderaNetConnection() override;

	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL,
	                                 EConnectionState InState, int32 InMaxPacket = 0,
	                                 int32 InPacketOverhead = 0) override;

	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL,
	                                  const class FInternetAddr& InRemoteAddr, EConnectionState InState,
	                                  int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;

	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	virtual FString LowLevelDescribe() override;
	virtual void CleanUp() override;

	/** Opaque backend implementation — defined per-backend in the corresponding .cpp */
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};

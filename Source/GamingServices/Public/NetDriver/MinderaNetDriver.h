#pragma once

#include "CoreMinimal.h"
#include "Engine/NetDriver.h"
#include "MinderaNetDriver.generated.h"

UCLASS(Transient)
class GAMINGSERVICES_API UMinderaNetDriver : public UNetDriver
{
	GENERATED_BODY()

public:
	virtual ~UMinderaNetDriver() override;

	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL,
	                      bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort,
	                        FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void Shutdown() override;
	virtual bool IsNetResourceValid() override;

	virtual ISocketSubsystem* GetSocketSubsystem() override;

	/** Opaque backend implementation — defined per-backend in the corresponding .cpp */
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};

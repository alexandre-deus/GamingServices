#pragma once

#include "CoreMinimal.h"
#include "GamingServiceResult.generated.h"

/** Base success/failure result shared by every async gaming-service operation. */
USTRUCT(BlueprintType)
struct FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	FGamingServiceResult() = default;

	FGamingServiceResult(bool InSuccess) : bSuccess(InSuccess)
	{
	}
};

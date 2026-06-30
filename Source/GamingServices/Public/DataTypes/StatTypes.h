#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "StatTypes.generated.h"

USTRUCT(BlueprintType)
struct FStatQueryResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString StatName;

	UPROPERTY(BlueprintReadOnly)
	int32 Value = 0;

	FStatQueryResult() = default;

	static FStatQueryResult Make(const FString& InName, int32 InValue)
	{
		FStatQueryResult R;
		R.bSuccess = true;
		R.StatName = InName;
		R.Value = InValue;
		return R;
	}
};

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "RemoteSettingTypes.generated.h"

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FRemoteSettingResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Key;

	UPROPERTY(BlueprintReadOnly)
	FString Value;

	FRemoteSettingResult() = default;

	FRemoteSettingResult(bool InSuccess, const FString& InKey = TEXT(""), const FString& InValue = TEXT(""))
		: FGamingServiceResult(InSuccess), Key(InKey), Value(InValue)
	{
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FRemoteSettingsListResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Keys;

	FRemoteSettingsListResult() = default;

	FRemoteSettingsListResult(bool InSuccess, const TArray<FString>& InKeys = TArray<FString>())
		: FGamingServiceResult(InSuccess), Keys(InKeys)
	{
	}
};

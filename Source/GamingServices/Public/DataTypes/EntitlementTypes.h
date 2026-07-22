#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "EntitlementTypes.generated.h"

USTRUCT(BlueprintType)
struct FEntitlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString Description;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FEntitlementDefinition
{
	GENERATED_BODY()

	/** Logical name used to look up this entitlement at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName LogicalName;

	/** EOS catalogue entitlement name (e.g. "premium_dlc") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EOSEntitlementName;

	/** Steam DLC or App AppId */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SteamAppId = 0;
};

USTRUCT(BlueprintType)
struct FEntitlementsListResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FEntitlement> Entitlements;

	FEntitlementsListResult() = default;

	FEntitlementsListResult(bool InSuccess,
	                        const TArray<FEntitlement>& InEntitlements = TArray<FEntitlement>())
		: FGamingServiceResult(InSuccess)
		  , Entitlements(InEntitlements)
	{
	}
};

USTRUCT(BlueprintType)
struct FHasEntitlementResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString EntitlementId;

	UPROPERTY(BlueprintReadOnly)
	bool bHasEntitlement = false;

	FHasEntitlementResult() = default;

	FHasEntitlementResult(bool InSuccess, const FString& InEntitlementId = TEXT(""), bool bInHasEntitlement = false)
		: FGamingServiceResult(InSuccess)
		  , EntitlementId(InEntitlementId)
		  , bHasEntitlement(bInHasEntitlement)
	{
	}
};

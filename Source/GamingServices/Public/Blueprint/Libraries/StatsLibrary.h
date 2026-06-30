#pragma once

#include "CoreMinimal.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/StatTypes.h"
#include "StatsLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStatIngestPin, const FGamingServiceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStatQueriedPin, const FStatQueryResult&, Result);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_IngestStat : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FStatIngestPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Stats", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_IngestStat* IngestStat(UObject* WorldContextObject, const FString& StatName, int32 Amount);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString StatName;

	UPROPERTY()
	int32 Amount = 0;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_QueryStat : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FStatQueriedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Stats", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_QueryStat* QueryStat(UObject* WorldContextObject, const FString& StatName);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString StatName;
};

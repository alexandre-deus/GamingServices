#pragma once

#include "CoreMinimal.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/LeaderboardTypes.h"
#include "LeaderboardsLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLeaderboardWritePin, const FGamingServiceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLeaderboardQueriedPin, const FLeaderboardResult&, Result);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_WriteLeaderboardScore : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FLeaderboardWritePin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Leaderboards", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_WriteLeaderboardScore* WriteLeaderboardScore(UObject* WorldContextObject, const FString& LeaderboardId, int32 Score);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString LeaderboardId;

	UPROPERTY()
	int32 Score = 0;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_QueryLeaderboardPage : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FLeaderboardQueriedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Leaderboards", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_QueryLeaderboardPage* QueryLeaderboardPage(UObject* WorldContextObject, const FString& LeaderboardId, int32 Limit, int32 ContinuationToken);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString LeaderboardId;

	UPROPERTY()
	int32 Limit = 0;

	UPROPERTY()
	int32 ContinuationToken = 0;
};

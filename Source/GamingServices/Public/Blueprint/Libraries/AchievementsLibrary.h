#pragma once

#include "CoreMinimal.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/AchievementTypes.h"
#include "AchievementsLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAchievementResultPin, const FGamingServiceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAchievementsQueriedPin, const FAchievementsQueryResult&, Result);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_UnlockAchievement : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FAchievementResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_UnlockAchievement* UnlockAchievement(UObject* WorldContextObject, const FString& AchievementId);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString AchievementId;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_QueryAchievements : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FAchievementsQueriedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_QueryAchievements* QueryAchievements(UObject* WorldContextObject);

	virtual void Activate() override;
};

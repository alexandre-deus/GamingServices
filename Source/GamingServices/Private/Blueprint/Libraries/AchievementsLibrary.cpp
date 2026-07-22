#include "Blueprint/Libraries/AchievementsLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IAchievementsService.h"

UAsyncAction_UnlockAchievement* UAsyncAction_UnlockAchievement::UnlockAchievement(UObject* WorldContextObject, const FString& AchievementId)
{
	UAsyncAction_UnlockAchievement* Action = NewObject<UAsyncAction_UnlockAchievement>();
	Action->WorldContext = WorldContextObject;
	Action->AchievementId = AchievementId;
	return Action;
}

void UAsyncAction_UnlockAchievement::Activate()
{
	IGamingService* Service = ResolveService();
	IAchievementsService* Achievements = Service ? Service->GetAchievements() : nullptr;
	if (!Achievements)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Achievements->UnlockAchievement(AchievementId, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_QueryAchievements* UAsyncAction_QueryAchievements::QueryAchievements(UObject* WorldContextObject)
{
	UAsyncAction_QueryAchievements* Action = NewObject<UAsyncAction_QueryAchievements>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_QueryAchievements::Activate()
{
	IGamingService* Service = ResolveService();
	IAchievementsService* Achievements = Service ? Service->GetAchievements() : nullptr;
	if (!Achievements)
	{
		Completed.Broadcast(FAchievementsQueryResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Achievements->QueryAchievements([this](const FAchievementsQueryResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

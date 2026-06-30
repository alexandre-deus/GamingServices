#pragma once

#include "CoreMinimal.h"
#include "DataTypes/AchievementTypes.h"
#include "Native/GamingCapability.h"

/** Achievement unlock + query capability. */
class GAMINGSERVICES_API IAchievementsService
{
public:
	virtual ~IAchievementsService() = default;

	virtual void UnlockAchievement(const FString& AchievementId,
	                               TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) = 0;
};

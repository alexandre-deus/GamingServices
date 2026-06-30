#pragma once

#include "CoreMinimal.h"
#include "DataTypes/LeaderboardTypes.h"
#include "Native/GamingCapability.h"

/** Leaderboard write + paged-query capability. */
class GAMINGSERVICES_API ILeaderboardsService
{
public:
	virtual ~ILeaderboardsService() = default;

	virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                                   TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
	                                  TFunction<void(const FLeaderboardResult&)> Callback) = 0;
};

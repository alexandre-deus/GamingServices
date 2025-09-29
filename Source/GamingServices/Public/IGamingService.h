#pragma once
#include "CoreMinimal.h"
#include "GamingServiceTypes.h"

class IGamingService
{
public:
	virtual ~IGamingService() = default;

	virtual bool Connect(const FGamingServiceConnectParams& Params) = 0;
	virtual void Shutdown() = 0;

	virtual void Login(const FGamingServiceLoginParams& Params,
	                   TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	virtual void UnlockAchievement(const FString& AchievementId,
	                               TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) = 0;
	virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                                   TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
	                                  TFunction<void(const FLeaderboardResult&)> Callback) = 0;

	virtual void IngestStat(const FString& StatName, int32 Amount,
	                        TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryStat(const FString& StatName,
	                       TFunction<void(const FStatQueryResult&)> Callback) = 0;

	virtual void Tick() = 0;
	virtual bool IsInitialized() const = 0;
	virtual bool IsLoggedIn() const = 0;
	virtual bool NeedsLogin() const = 0;
	virtual FString GetUserId() const = 0;
	virtual FString GetDisplayName() const = 0;

	template <class T>
	T& GetServiceAs() { return *static_cast<T*>(this); }
};

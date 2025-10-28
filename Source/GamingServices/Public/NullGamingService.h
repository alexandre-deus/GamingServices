#pragma once

#include "CoreMinimal.h"
#include "FGamingService.h"
#include "GamingServiceTypes.h"

class GAMINGSERVICES_API FNullGamingService : public FGamingService
{
public:
	FNullGamingService();
	virtual ~FNullGamingService() override;

	virtual bool Connect(const FGamingServiceConnectParams& Params) override;
	virtual void Shutdown() override;

	virtual void Login(const FGamingServiceLoginParams& Params,
	                   TFunction<void(const FGamingServiceResult&)> Callback) override;

	virtual void UnlockAchievement(const FString& AchievementId,
	                               TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) override;
	virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                                   TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
	                                  TFunction<void(const FLeaderboardResult&)> Callback) override;

	virtual void IngestStat(const FString& StatName, int32 Amount,
	                        TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void QueryStat(const FString& StatName,
	                       TFunction<void(const FStatQueryResult&)> Callback) override;

	virtual void WriteFile(const FString& FilePath, const TArray<uint8>& Data,
	                       TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void ReadFile(const FString& FilePath,
	                      TFunction<void(const FFileReadResult&)> Callback) override;
	virtual void DeleteFile(const FString& FilePath,
	                        TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void ListFiles(const FString& DirectoryPath,
	                       TFunction<void(const FFilesListResult&)> Callback) override;

	virtual void Tick() override;

	virtual bool IsInitialized() const override;
	virtual bool IsLoggedIn() const override;
	virtual bool NeedsLogin() const override;
	virtual FString GetUserId() const override;
	virtual FString GetDisplayName() const override;

private:
	bool bInitialized;
	bool bLoggedIn;
};

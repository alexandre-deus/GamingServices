#pragma once

#include "CoreMinimal.h"
#include "Services/FGamingService.h"
#include "GamingServiceTypes.h"

class GAMINGSERVICES_API FSteamworksGamingService : public FGamingService
{
public:
	FSteamworksGamingService();
	virtual ~FSteamworksGamingService() override;

	virtual void InitializePlatform() override;
	virtual void DestroyPlatform() override;

	virtual void Login(const FGamingServiceLoginParams& Params,
	                   TFunction<void(const FGamingServiceResult&)> Callback) override;

	virtual void UnlockAchievement(const FString& AchievementId,
	                               TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) override;
	virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                                   TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
	                                  TFunction<void(const FLeaderboardResult&)> Callback) override;

	virtual void ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback) override;
	virtual void HasEntitlement(const FEntitlementDefinition& Definition,
	                            TFunction<void(const FHasEntitlementResult&)> Callback) override;
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

	virtual void CreateSession(const FSessionSettings& Settings,
	                          TFunction<void(const FSessionCreateResult&)> Callback) override;
	virtual void FindSessions(const FSessionSearchFilter& Filter,
	                         TFunction<void(const FSessionSearchResult&)> Callback) override;
	virtual void JoinSession(const FSessionJoinHandle& JoinHandle,
	                        TFunction<void(const FSessionJoinResult&)> Callback) override;
	virtual void LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void DestroySession(TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void UpdateSession(const FSessionSettings& Settings,
	                          TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void LockLobby(TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback) override;

	virtual void ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback) override;

	virtual FString GetSessionConnectionString() const override;

	virtual void Tick() override;

	virtual bool IsInitialized() const override;
	virtual bool IsLoggedIn() const override;
	virtual bool NeedsLogin() const override;
	virtual FString GetUserId() const override;
	virtual FString GetDisplayName() const override;

	bool IsSteamRunning() const;
	bool IsSteamOverlayEnabled() const;

private:
	class FSteamworksGamingServiceImpl;
	TUniquePtr<FSteamworksGamingServiceImpl> Impl;
};

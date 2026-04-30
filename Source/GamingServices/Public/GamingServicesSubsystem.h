#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamingServiceTypes.h"
#include "Services/FGamingService.h"
#include "GamingServicesSubsystem.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLoggedIn, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingAchievementUnlocked, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingAchievementsQueried, const FAchievementsQueryResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingEntitlementsListed, const FEntitlementsListResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingEntitlementChecked, const FHasEntitlementResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLeaderboardScoreWritten, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLeaderboardQueried, const FLeaderboardResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingStatIngested, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingStatQueried, const FStatQueryResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingFileWritten, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingFileRead, const FFileReadResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingFileDeleted, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingFilesListed, const FFilesListResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingRemoteSettingChanged, const FRemoteSettingResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingRemoteSettingQueried, const FRemoteSettingResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingRemoteSettingDeleted, const FRemoteSettingResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingRemoteSettingsListed, const FRemoteSettingsListResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionCreated, const FSessionCreateResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionsFound, const FSessionSearchResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionJoined, const FSessionJoinResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionLeft, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionEnded, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionDestroyed, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionUpdated, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLobbyLocked, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLobbyUnlocked, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingInviteFriendsDialogShown, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionUserJoined, const FSessionMemberInfo&, MemberInfo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingSessionUserLeft, const FSessionMemberInfo&, MemberInfo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLobbyInviteAccepted, const FLobbyInviteAcceptedInfo&, InviteInfo);

UCLASS()
class GAMINGSERVICES_API UGamingServicesSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Achievement API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements")
	void UnlockAchievement(const FString& AchievementId);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements")
	void QueryAchievements();

	// Entitlements API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Entitlements")
	void RegisterEntitlement(const FEntitlementDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Entitlements")
	void ListEntitlements();

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Entitlements")
	void HasEntitlement(FName LogicalName);

	// Leaderboards API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Leaderboards")
	void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Leaderboards")
	void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken);

	// Stats API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Stats")
	void IngestStat(const FString& StatName, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Stats")
	void QueryStat(const FString& StatName);

	// Remote Storage API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud")
	void WriteFile(const FString& FilePath, const TArray<uint8>& Data);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud")
	void ReadFile(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud")
	void DeleteFile(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud")
	void ListFiles(const FString& DirectoryPath);

	// Remote Settings API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings")
	void SetRemoteSetting(const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings")
	void GetRemoteSetting(const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings")
	void DeleteRemoteSetting(const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings")
	void ListRemoteSettings();

	// Matchmaking API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void CreateSession(const FSessionSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void FindSessions(const FSessionSearchFilter& Filter);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void JoinSession(const FSessionJoinHandle& JoinHandle);
	
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void LeaveSession();

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void DestroySession();

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void UpdateSession(const FSessionSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void LockLobby();

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void UnlockLobby();

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void GetCurrentSessionInfo();

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking")
	void ShowInviteFriendsDialog();

	UFUNCTION(BlueprintPure, Category = "GamingServices|Matchmaking")
	FString GetSessionConnectionString() const;

	// Auth/query helpers for Blueprints
	UFUNCTION(BlueprintPure, Category = "GamingServices")
	bool IsConnected() const;

	UFUNCTION(BlueprintPure, Category = "GamingServices")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintPure, Category = "GamingServices")
	bool NeedsLogin() const;

	UFUNCTION(BlueprintPure, Category = "GamingServices|User")
	FString GetUserId() const;

	UFUNCTION(BlueprintPure, Category = "GamingServices|User")
	FString GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "GamingServices|User")
	UTexture2D* GetAvatar() const;

	UFUNCTION(BlueprintCallable, Category = "GamingServices")
	void Login(const FGamingServiceLoginParams& Params);

	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLoggedIn OnLoggedIn;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingAchievementUnlocked OnAchievementUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingAchievementsQueried OnAchievementsQueried;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingEntitlementsListed OnEntitlementsListed;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingEntitlementChecked OnEntitlementChecked;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLeaderboardScoreWritten OnLeaderboardScoreWritten;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLeaderboardQueried OnLeaderboardQueried;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingStatIngested OnStatIngested;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingStatQueried OnStatQueried;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingFileWritten OnFileWritten;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingFileRead OnFileRead;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingFileDeleted OnFileDeleted;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingFilesListed OnFilesListed;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingRemoteSettingChanged OnRemoteSettingChanged;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingRemoteSettingQueried OnRemoteSettingQueried;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingRemoteSettingDeleted OnRemoteSettingDeleted;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingRemoteSettingsListed OnRemoteSettingsListed;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionCreated OnSessionCreated;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionsFound OnSessionsFound;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionJoined OnSessionJoined;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionLeft OnSessionLeft;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionEnded OnSessionEnded;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionDestroyed OnSessionDestroyed;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionUpdated OnSessionUpdated;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLobbyLocked OnLobbyLocked;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLobbyUnlocked OnLobbyUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingInviteFriendsDialogShown OnInviteFriendsDialogShown;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionUserJoined OnSessionUserJoined;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingSessionUserLeft OnSessionUserLeft;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLobbyInviteAccepted OnLobbyInviteAccepted;

	FGamingService& GetService() const { return *Service; }

private:
	FGamingService* Service = nullptr;
	TMap<FName, FEntitlementDefinition> EntitlementCatalog;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Service != nullptr; }
};

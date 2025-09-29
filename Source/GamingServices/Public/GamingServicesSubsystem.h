#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamingServiceTypes.h"
#include "IGamingService.h"
#include "GamingServicesSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLoggedIn, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingAchievementUnlocked, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingAchievementsQueried, const FAchievementsQueryResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLeaderboardScoreWritten, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingLeaderboardQueried, const FLeaderboardResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingStatProgressed, const FGamingServiceResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamingStatQueried, const FStatQueryResult&, Result);

UCLASS()
class GAMINGSERVICES_API UGamingServicesSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Connection / initialization
	UFUNCTION(BlueprintCallable, Category = "GamingServices")
	bool Connect(const FGamingServiceConnectParams& Params);

	// Achievement API
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements")
	void UnlockAchievement(const FString& AchievementId);

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements")
	void QueryAchievements();

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

	// Auth/query helpers for Blueprints
	UFUNCTION(BlueprintPure, Category = "GamingServices")
	bool IsConnected() const;

	UFUNCTION(BlueprintPure, Category = "GamingServices")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintPure, Category = "GamingServices")
	bool NeedsLogin() const;

	UFUNCTION(BlueprintCallable, Category = "GamingServices")
	void Login(const FGamingServiceLoginParams& Params);

	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLoggedIn OnLoggedIn;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingAchievementUnlocked OnAchievementUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingAchievementsQueried OnAchievementsQueried;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLeaderboardScoreWritten OnLeaderboardScoreWritten;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingLeaderboardQueried OnLeaderboardQueried;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingStatProgressed OnStatProgressed;
	UPROPERTY(BlueprintAssignable, Category = "GamingServices|Events")
	FOnGamingStatQueried OnStatQueried;

	IGamingService& GetService() const { return *Service; }

private:
	TUniquePtr<IGamingService> Service;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Service != nullptr; }
};

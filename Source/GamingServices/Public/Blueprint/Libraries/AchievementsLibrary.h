#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/AchievementTypes.h"
#include "AchievementsLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAchievementResultPin, const FGamingServiceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAchievementsQueriedPin, const FAchievementsQueryResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAchievementProgressPin, const FAchievementProgressResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAchievementUnlockedPin, const FGameAchievement&, Achievement);

/**
 * Unlock an achievement outright.
 *
 * The id is the game's own id when a catalogue has been registered (see RegisterAchievements), and the
 * raw platform id otherwise — with nothing registered the two are the same thing. Unlocking something
 * already unlocked succeeds and changes nothing.
 */
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

/** Read every achievement and this player's state for each, refreshing the cache behind the getters. */
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

/**
 * Report a counted achievement's ABSOLUTE progress and unlock it if that reaches the target.
 *
 * Report the player's running total ("42 monsters killed so far"), not an increment — a total lower
 * than the one already stored is a no-op success, so reporting the same total repeatedly is safe and
 * nothing is ever double-counted. Requires the achievement to be registered with a stat and a target.
 */
UCLASS()
class GAMINGSERVICES_API UAsyncAction_ReportAchievementProgress : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FAchievementProgressPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ReportAchievementProgress* ReportAchievementProgress(UObject* WorldContextObject,
	                                                                        const FString& AchievementId,
	                                                                        int32 CurrentValue);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString AchievementId;

	UPROPERTY()
	int32 CurrentValue = 0;
};

/**
 * Add to a counted achievement's progress ("+1 monster") and unlock it if the total reaches the target.
 *
 * Use this when the game knows the increment rather than the running total. Every call moves the stat,
 * so call it once per event — unlike ReportAchievementProgress, calling it twice for the same event
 * counts that event twice.
 */
UCLASS()
class GAMINGSERVICES_API UAsyncAction_AddAchievementProgress : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FAchievementProgressPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_AddAchievementProgress* AddAchievementProgress(UObject* WorldContextObject,
	                                                                  const FString& AchievementId, int32 Delta);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString AchievementId;

	UPROPERTY()
	int32 Delta = 0;
};

/**
 * Check the registered catalogue against what the live platform publishes, and log every disagreement:
 * ids the platform does not have, targets that no longer match its thresholds, stats it does not gate
 * on. Development tool — run it once after sign-in while building the achievement list.
 */
UCLASS()
class GAMINGSERVICES_API UAsyncAction_ValidateAchievements : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FAchievementResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ValidateAchievements* ValidateAchievements(UObject* WorldContextObject);

	virtual void Activate() override;
};

/**
 * Fires whenever the platform reports an achievement unlocked — including unlocks this game never
 * asked for, such as an EOS server-side threshold or an unlock from another device. Drive the
 * "achievement earned" toast from here rather than from the unlock call's own result, so it appears
 * once regardless of who did the unlocking.
 *
 * This node stays active for as long as the object that created it lives.
 */
UCLASS()
class GAMINGSERVICES_API UAsyncAction_ListenForAchievementUnlocks : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FAchievementUnlockedPin Unlocked;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ListenForAchievementUnlocks* ListenForAchievementUnlocks(UObject* WorldContextObject);

	virtual void Activate() override;
};

/** Achievement registration and synchronous getters (no platform round-trip). */
UCLASS()
class GAMINGSERVICES_API UAchievementsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Register the game's achievements: one row per achievement, mapping the game's own id onto each
	 * platform's id and naming the stat that counts it.
	 *
	 * Pure data with no platform calls behind it, so it is safe at any point — before sign-in, before
	 * the SDK is up. Register once at start-up (from a data asset, a table, or literals) and every
	 * other achievement node can then be given the game's own ids on every platform.
	 *
	 * Calling it again merges by Id: an existing row is replaced, the rest are left alone.
	 */
	UFUNCTION(BlueprintCallable, Category = "GamingServices|Achievements", meta = (WorldContext = "WorldContextObject"))
	static void RegisterAchievements(const UObject* WorldContextObject,
	                                 const TArray<FAchievementDefinition>& Definitions);

	/** Everything registered so far, in registration order. */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements", meta = (WorldContext = "WorldContextObject"))
	static TArray<FAchievementDefinition> GetRegisteredAchievements(const UObject* WorldContextObject);

	/**
	 * The id the live platform knows this achievement by. Returns the input unchanged when nothing is
	 * registered under it. Useful for logging and for talking to platform tooling directly.
	 */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements", meta = (WorldContext = "WorldContextObject"))
	static FString ResolvePlatformAchievementId(const UObject* WorldContextObject, const FString& AchievementId);

	/** Whether achievements can be read or written right now (SDK up, signed in, definitions loaded). */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements", meta = (WorldContext = "WorldContextObject"))
	static bool AreAchievementsReady(const UObject* WorldContextObject);

	/** Last queried achievements, without re-querying. Empty before the first QueryAchievements. */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGameAchievement> GetCachedAchievements(const UObject* WorldContextObject);

	/** One cached achievement by id. Returns false (and a default struct) when it is not known yet. */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements", meta = (WorldContext = "WorldContextObject"))
	static bool GetCachedAchievement(const UObject* WorldContextObject, const FString& AchievementId,
	                                 FGameAchievement& OutAchievement);

	/**
	 * Typed count / element accessors for an achievement array, matching the friends library: concrete
	 * pins instead of the generic array nodes' wildcards, so an iterating graph is unambiguous.
	 */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements")
	static int32 GetAchievementCount(const TArray<FGameAchievement>& Achievements);

	/** Returns a default-constructed achievement when Index is out of range. */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements")
	static FGameAchievement GetAchievementAt(const TArray<FGameAchievement>& Achievements, int32 Index);

	/** Progress as a 0..1 fraction of this player's own completion (never the achievement's rarity). */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Achievements")
	static float GetAchievementProgressFraction(const FGameAchievement& Achievement);
};

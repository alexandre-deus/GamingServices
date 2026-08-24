#pragma once

#include "CoreMinimal.h"
#include "DataTypes/AchievementTypes.h"
#include "Native/GamingCapability.h"

/**
 * Registered-catalogue achievements: one id per achievement across every backend, and one call to
 * move a progressive achievement forward.
 *
 * This is the game-facing half of achievements, layered over the platform's own IAchievementsService
 * the same way IRemoteSettingsService is layered over ICloudStorageService — see
 * FAchievementProgressStore for the shared, backend-agnostic implementation every backend reuses.
 *
 * Two things it adds over the raw platform interface:
 *
 *   1. Id mapping. Gameplay code says "Monster100"; the store resolves that to the Steam API Name or
 *      the EOS AchievementId of whichever backend is live. With nothing registered it passes ids
 *      through unchanged, so adopting the catalogue is optional and incremental.
 *
 *   2. A single progress write. ReportProgress() is all a game needs for a counted achievement,
 *      whichever platform it is running on — the store ingests the stat, drives the platform's
 *      progress toast, and unlocks where the platform will not unlock by itself.
 */
class GAMINGSERVICES_API IAchievementProgressService
{
public:
	virtual ~IAchievementProgressService() = default;

	/**
	 * Register (or re-register) achievement definitions. Rows are merged by Id, so calling this again
	 * with an existing Id replaces that row and leaves the rest alone.
	 *
	 * Registration is pure data with no platform calls behind it, so it is safe at any point —
	 * before login, before the SDK is up, from a data asset at game-instance init.
	 */
	virtual void RegisterAchievements(const TArray<FAchievementDefinition>& Definitions) = 0;

	/** Everything registered so far, in registration order. */
	virtual const TArray<FAchievementDefinition>& GetRegisteredAchievements() const = 0;

	/** One registered definition by the game's own id. False when it was never registered. */
	virtual bool FindDefinition(const FString& Id, FAchievementDefinition& OutDefinition) const = 0;

	/**
	 * The platform id the live backend knows this achievement by. Returns Id unchanged when nothing
	 * is registered under it, which is what makes an empty catalogue a pass-through.
	 */
	virtual FString ResolvePlatformId(const FString& Id) const = 0;

	/** Unlock outright by registered id, ignoring progression. Idempotent. */
	virtual void UnlockAchievement(const FString& Id,
	                               TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	/**
	 * Set a progressive achievement's counter to an ABSOLUTE value and unlock it if that reaches the
	 * target. Reporting a value at or below the one already stored is a no-op success, so a caller
	 * can report the player's running total every time without ever double-counting or going
	 * backwards.
	 *
	 * On an achievement registered without a stat this behaves as an unlock once CurrentValue is at
	 * least the target, and fails when there is no target to compare against.
	 */
	virtual void ReportProgress(const FString& Id, int32 CurrentValue,
	                            TFunction<void(const FAchievementProgressResult&)> Callback) = 0;

	/**
	 * Add to a progressive achievement's counter. Convenience over ReportProgress for callers that
	 * know the increment ("+1 monster") rather than the running total; the store reads the stored
	 * value and reports the sum.
	 */
	virtual void AddProgress(const FString& Id, int32 Delta,
	                         TFunction<void(const FAchievementProgressResult&)> Callback) = 0;

	/** Cached platform state for a registered achievement. False when unknown or not yet queried. */
	virtual bool GetAchievement(const FString& Id, FGameAchievement& OutAchievement) const = 0;

	/** Re-read every achievement from the platform and refresh the cache both layers share. */
	virtual void Refresh(TFunction<void(const FAchievementsQueryResult&)> Callback) = 0;

	/**
	 * Compare the registered catalogue against what the live backend actually publishes and report
	 * the disagreements: ids that exist here but not on the platform, platform achievements missing
	 * from the catalogue, and targets that no longer match the platform's own threshold.
	 *
	 * Portal configuration drifts silently and the symptom is an achievement that simply never fires
	 * in a shipped build. Call this once after login in development builds; every problem it finds is
	 * logged as a warning naming the achievement.
	 */
	virtual void ValidateAgainstPlatform(TFunction<void(const FGamingServiceResult&)> Callback) = 0;
};

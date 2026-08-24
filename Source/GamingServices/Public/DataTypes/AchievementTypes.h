#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "AchievementTypes.generated.h"

/**
 * One stat an achievement is gated on, with the player's standing against it.
 *
 * Both platforms drive progressive achievements from stats, and both can report the pair back:
 * EOS as EOS_Achievements_PlayerStatInfo, Steam as the progress limits of the achievement's
 * associated stat. An achievement with no progression has an empty StatProgress array.
 */
USTRUCT(BlueprintType)
struct FAchievementStatProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString StatName;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentValue = 0;

	/** Value CurrentValue must reach for this stat's requirement to be satisfied. */
	UPROPERTY(BlueprintReadOnly)
	int32 ThresholdValue = 0;
};

/**
 * A single achievement as the platform reports it.
 *
 * Id is the PLATFORM's id (Steam API Name / EOS AchievementId). Games that registered a catalogue
 * with IAchievementProgressService address achievements by their own id instead and let the store
 * map it; see FAchievementDefinition.
 */
USTRUCT(BlueprintType)
struct FGameAchievement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	bool bIsUnlocked = false;

	/**
	 * THIS PLAYER's progress towards unlocking, 0..1. 1.0 whenever bIsUnlocked.
	 *
	 * Derived from CurrentValue/TargetValue where the platform exposes them, so it means the same
	 * thing on every backend. It is NOT how many players own the achievement — that is
	 * GlobalUnlockPercent, which is a different number entirely.
	 */
	UPROPERTY(BlueprintReadOnly)
	double Progress = 0.0;

	/** Progress numerator in the achievement's own units (kills, runs, ...). 0 when not progressive. */
	UPROPERTY(BlueprintReadOnly)
	int32 CurrentValue = 0;

	/** Value CurrentValue must reach to unlock. 0 when the platform reports no progression. */
	UPROPERTY(BlueprintReadOnly)
	int32 TargetValue = 0;

	/** When the player unlocked it. Zero (FDateTime()) while locked — test bIsUnlocked, not this. */
	UPROPERTY(BlueprintReadOnly)
	FDateTime UnlockTime;

	/**
	 * Share of the game's players who have unlocked this, 0..100 — the achievement's rarity, not the
	 * local player's progress. -1 when the platform has not supplied it (EOS has no equivalent, and
	 * Steam only fills it in after its global percentages have been requested and delivered).
	 */
	UPROPERTY(BlueprintReadOnly)
	float GlobalUnlockPercent = -1.0f;

	/** Hidden achievements should have their name/description withheld until unlocked. */
	UPROPERTY(BlueprintReadOnly)
	bool bIsHidden = false;

	/** Platform-hosted icon, when the store front provides one as a URL. Empty on Steam (see below). */
	UPROPERTY(BlueprintReadOnly)
	FString IconUrl;

	/** Free-form text configured alongside the achievement. Empty when unset or unsupported. */
	UPROPERTY(BlueprintReadOnly)
	FString FlavorText;

	/** Per-stat breakdown behind CurrentValue/TargetValue. Empty for non-progressive achievements. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FAchievementStatProgress> StatProgress;

	/** Progress as a 0..100 percentage, for UI that would otherwise multiply it itself. */
	float GetProgressPercent() const { return static_cast<float>(Progress) * 100.0f; }
};

USTRUCT(BlueprintType)
struct FAchievementsQueryResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FGameAchievement> Achievements;

	FAchievementsQueryResult() = default;

	FAchievementsQueryResult(bool InSuccess,
	                         const TArray<FGameAchievement>& InAchievements = TArray<FGameAchievement>())
		: FGamingServiceResult(InSuccess)
		  , Achievements(InAchievements)
	{
	}
};

/**
 * One achievement's mapping from the game's own id onto each platform's id, plus the stat that
 * drives it. Registered with IAchievementProgressService; see FAchievementProgressStore.
 */
USTRUCT(BlueprintType)
struct FAchievementDefinition
{
	GENERATED_BODY()

	/** The game's own id. This is what gameplay code passes around; it never changes per platform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Id;

	/**
	 * Steam API Name from the partner site.
	 *
	 * Empty means one of two things, decided by whether the row names ANY platform id:
	 *   - No platform id set at all → the achievement uses Id verbatim on every platform.
	 *   - Some other platform id set → the achievement does not exist on this one. Calls against it
	 *     are a no-op success there, so gameplay code never has to branch per platform.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SteamApiName;

	/** EOS AchievementId from the Dev Portal. Empty follows the same rule as SteamApiName above. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EOSAchievementId;

	/**
	 * Stat this achievement counts, for progressive achievements. Must be the same stat name the
	 * platform's own configuration gates on: Steam's associated stat, EOS's StatThreshold name.
	 * Empty means the achievement is unlocked outright rather than counted up to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString StatName;

	/**
	 * Value StatName must reach to unlock. Ignored when StatName is empty.
	 *
	 * EOS holds this threshold server-side and unlocks by itself; Steam has no server-side rule and
	 * needs the game to unlock at the threshold, which is why the number is also kept here. Keep it
	 * equal to the EOS Dev Portal threshold — FAchievementProgressStore::ValidateAgainstPlatform
	 * checks that for you and logs the ones that have drifted.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TargetValue = 0;

	bool IsProgressive() const { return !StatName.IsEmpty() && TargetValue > 0; }
};

/** Outcome of reporting progress: where the player stands now, and whether that unlocked anything. */
USTRUCT(BlueprintType)
struct FAchievementProgressResult : public FGamingServiceResult
{
	GENERATED_BODY()

	/** The game's own achievement id, as registered. */
	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentValue = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetValue = 0;

	/** True only on the call that crossed the threshold, so a caller can fire its own toast once. */
	UPROPERTY(BlueprintReadOnly)
	bool bUnlockedNow = false;

	FAchievementProgressResult() = default;

	FAchievementProgressResult(bool InSuccess, const FString& InId, int32 InCurrent = 0, int32 InTarget = 0,
	                           bool bInUnlockedNow = false)
		: FGamingServiceResult(InSuccess)
		  , Id(InId)
		  , CurrentValue(InCurrent)
		  , TargetValue(InTarget)
		  , bUnlockedNow(bInUnlockedNow)
	{
	}
};

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/AchievementTypes.h"
#include "Native/GamingCapability.h"

/**
 * The platform's own achievement API, exposed as far as the backends can actually implement it.
 *
 * Ids here are PLATFORM ids — a Steam API Name or an EOS AchievementId, whichever backend is live.
 * Gameplay code that wants one id per achievement across both stores should talk to
 * IAchievementProgressService instead, which maps a registered catalogue onto this interface.
 *
 * Capability differs by backend and is reported honestly rather than emulated:
 *
 *   - IndicateProgress is Steam's progress toast. EOS has no equivalent notion (its server unlocks
 *     from stat thresholds on its own), so it reports unsupported instead of pretending.
 *   - GlobalUnlockPercent is Steam-only, and only after QueryAchievements has fetched the global
 *     percentages. EOS leaves it at -1.
 *   - ResetAchievements is Steam-only and destructive; EOS has no client-side reset at all.
 *   - IconUrl is EOS-only. Steam serves icons as image handles through ISteamUtils rather than URLs,
 *     so it leaves the field empty rather than inventing a URL that resolves to nothing.
 *
 * Everything that is unsupported has a default implementation here that fails or reports empty, so a
 * backend implements only what its SDK really offers and callers see a plain "no" at the source.
 */
class GAMINGSERVICES_API IAchievementsService
{
public:
	virtual ~IAchievementsService() = default;

	/**
	 * Unlock outright, ignoring any progression the achievement has. Idempotent on both backends:
	 * unlocking an already-unlocked achievement succeeds and does nothing.
	 */
	virtual void UnlockAchievement(const FString& AchievementId,
	                               TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	/**
	 * Fetch definitions plus this player's state for all of them, and refresh the cache behind
	 * GetCachedAchievements(). The callback fires exactly once.
	 */
	virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) = 0;

	/** Whether the backend can serve achievement calls right now (SDK up, signed in, data loaded). */
	virtual bool IsReady() const { return false; }

	/**
	 * Last queried achievements, without re-querying. Empty before the first successful
	 * QueryAchievements. Cheap enough to call from UI; bind OnAchievementsChanged to know when to
	 * re-read it.
	 */
	virtual const TArray<FGameAchievement>& GetCachedAchievements() const
	{
		static const TArray<FGameAchievement> Empty;
		return Empty;
	}

	/** One cached achievement by platform id. False when unknown or nothing has been queried yet. */
	virtual bool FindCachedAchievement(const FString& AchievementId, FGameAchievement& OutAchievement) const
	{
		for (const FGameAchievement& Achievement : GetCachedAchievements())
		{
			if (Achievement.Id == AchievementId)
			{
				OutAchievement = Achievement;
				return true;
			}
		}
		return false;
	}

	/**
	 * Show the platform's progress notification ("37 of 50") without unlocking anything.
	 *
	 * Steam only, and Steam rate-limits it — call it on milestones, not on every increment, or the
	 * toasts are dropped and the player is spammed. Reports failure on backends that have no such
	 * concept; that failure is not an error worth surfacing to a player.
	 */
	virtual void IndicateProgress(const FString& AchievementId, int32 CurrentValue, int32 MaxValue,
	                              TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (Callback)
		{
			Callback(FGamingServiceResult(false));
		}
	}

	/**
	 * Wipe this player's achievements. Steam only, and only ever a development tool — do not ship a
	 * path that reaches this.
	 *
	 * Steam has no achievements-only reset (ResetAllStats always clears the stats as well), so this
	 * clears both. That is unavoidable rather than a choice, and it is the other reason this is not
	 * something to put in front of a player.
	 */
	virtual void ResetAchievements(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (Callback)
		{
			Callback(FGamingServiceResult(false));
		}
	}

	/**
	 * Fires when the platform reports an achievement was unlocked — including unlocks this game did
	 * not ask for, such as an EOS server-side threshold unlock or an unlock from another device.
	 * Carries the achievement as cached at that moment, so a toast can be driven straight off it.
	 */
	TFunction<void(const FGameAchievement&)> OnAchievementUnlocked;

	/**
	 * Fires when cached achievement state changed for any other reason (a query landed, progress
	 * moved). Carries no payload: re-read GetCachedAchievements().
	 */
	TFunction<void()> OnAchievementsChanged;
};

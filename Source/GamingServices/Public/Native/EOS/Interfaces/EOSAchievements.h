#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IAchievementsService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/**
	 * EOS achievements over the shared platform core.
	 *
	 * The definitions are prefetched once during login (FEOSPlatformCore::LoadAchievementDefinitions),
	 * so QueryAchievements only has to fetch this player's progress and join the two together. Both
	 * halves are needed: the definition carries the achievement's configuration (name, icons, hidden
	 * flag, stat thresholds) and the player record carries their standing against it.
	 *
	 * EOS unlocks progressive achievements on its own server once the ingested stats cross the
	 * configured thresholds, so an unlock can arrive without this game ever asking for one — from
	 * another device, or from a stat written by a different build. That is what the unlocked
	 * notification is for, and why it is registered for the whole session rather than around calls.
	 *
	 * Two fields on FGameAchievement have no EOS equivalent and stay at their defaults:
	 * GlobalUnlockPercent (EOS publishes no rarity data) and the progress toast behind
	 * IndicateProgress (EOS drives its own notification when the server unlocks).
	 */
	class FEOSAchievements final : public IAchievementsService
	{
	public:
		explicit FEOSAchievements(FEOSPlatformCore& InCore);
		virtual ~FEOSAchievements() override;

		virtual void UnlockAchievement(const FString& AchievementId,
		                               TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) override;

		virtual bool IsReady() const override;
		virtual const TArray<FGameAchievement>& GetCachedAchievements() const override { return Cached; }

	private:
		/**
		 * Subscribes to EOS_Achievements_AddNotifyAchievementsUnlockedV2. Driven by the core's login /
		 * shutdown sequence through its hooks, because the handle and ProductUserId this needs only
		 * exist between those two points.
		 */
		void RegisterUnlockNotifications();
		void UnregisterUnlockNotifications();

		/** Rebuilds Cached from the core's definitions joined with this player's achievement records. */
		void RebuildCache();

		/** Reads one achievement back out of EOS and reports it to OnAchievementUnlocked. */
		void ReportUnlocked(const FString& AchievementId, int64 UnlockTimeUnix);

		FEOSPlatformCore& Core;

		TArray<FGameAchievement> Cached;

		/** EOS_NotificationId as a plain integer so this header stays SDK-free. 0 means not registered. */
		uint64 UnlockNotificationId = 0;
	};
}

#endif // GS_WITH_EOS

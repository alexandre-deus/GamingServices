#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IAchievementsService.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/**
	 * Steam achievements, read and written through the global SteamUserStats() interface.
	 *
	 * Steam keeps the local user's achievement state in the client, so reads are synchronous and
	 * QueryAchievements completes on the calling frame. The one asynchronous part is the global unlock
	 * percentages, which must be requested from Steam's servers before GetAchievementAchievedPercent
	 * returns anything at all — QueryAchievements kicks that off and fills GlobalUnlockPercent in on a
	 * later query once it lands, rather than blocking the first one on a network round trip.
	 *
	 * Steam has no server-side rule that unlocks an achievement when a stat crosses a threshold: the
	 * game must call UnlockAchievement itself. FAchievementProgressStore is what does that, so games
	 * do not have to know which platform they are on.
	 *
	 * Progress reporting is deliberately partial and honest. GetAchievementProgressLimits gives the
	 * target for an achievement with an associated progress stat, but Steam exposes no way to ask
	 * which stat that is, so a locked achievement's current value is only known once it moves and
	 * Steam reports it through UserAchievementStored_t. Games that need progress before then should
	 * read it through IAchievementProgressService, which knows the stat from the registered catalogue.
	 *
	 * The Steam SDK and its callbacks live in a private FImpl so this header stays SDK-free.
	 */
	class FSteamAchievements final : public IAchievementsService
	{
	public:
		explicit FSteamAchievements(FSteamPlatformCore& InCore);
		virtual ~FSteamAchievements() override;

		virtual void UnlockAchievement(const FString& AchievementId,
		                               TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) override;

		virtual bool IsReady() const override;
		virtual const TArray<FGameAchievement>& GetCachedAchievements() const override;
		virtual void IndicateProgress(const FString& AchievementId, int32 CurrentValue, int32 MaxValue,
		                              TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void ResetAchievements(TFunction<void(const FGamingServiceResult&)> Callback) override;

	private:
		struct FImpl;

		FSteamPlatformCore& Core;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_STEAM

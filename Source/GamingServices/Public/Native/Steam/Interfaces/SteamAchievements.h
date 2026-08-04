#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IAchievementsService.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/** Steam-backed achievement unlock + query via the global SteamUserStats() interface. */
	class FSteamAchievements final : public IAchievementsService
	{
	public:
		explicit FSteamAchievements(FSteamPlatformCore& InCore) : Core(InCore) {}

		virtual void UnlockAchievement(const FString& AchievementId,
		                               TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) override;

	private:
		FSteamPlatformCore& Core;
	};
}

#endif // GS_WITH_STEAM

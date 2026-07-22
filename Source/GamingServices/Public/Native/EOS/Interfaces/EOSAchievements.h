#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IAchievementsService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/** EOS achievements capability (unlock + player-achievement query) over the shared platform core. */
	class FEOSAchievements final : public IAchievementsService
	{
	public:
		explicit FEOSAchievements(FEOSPlatformCore& InCore) : Core(InCore) {}

		virtual void UnlockAchievement(const FString& AchievementId,
		                               TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) override;

	private:
		FEOSPlatformCore& Core;
	};
}

#endif // USE_EOS

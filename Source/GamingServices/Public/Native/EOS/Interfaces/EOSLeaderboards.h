#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/ILeaderboardsService.h"

namespace GamingServices
{
	class FEOSPlatformCore;
	class FEOSStats;

	/**
	 * EOS leaderboards capability over the shared platform core.
	 *
	 * Writing a leaderboard score resolves the backing stat from the cached leaderboard definition and
	 * ingests it through the stats capability, exactly as the legacy backend did.
	 */
	class FEOSLeaderboards final : public ILeaderboardsService
	{
	public:
		FEOSLeaderboards(FEOSPlatformCore& InCore, FEOSStats& InStats) : Core(InCore), Stats(InStats) {}

		virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
		                                   TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 Offset,
		                                  TFunction<void(const FLeaderboardResult&)> Callback) override;
		virtual void QueryLeaderboardUserRank(const FString& LeaderboardId,
		                                      TFunction<void(const FLeaderboardResult&)> Callback) override;

	private:
		FEOSPlatformCore& Core;
		FEOSStats& Stats;
	};
}

#endif // USE_EOS

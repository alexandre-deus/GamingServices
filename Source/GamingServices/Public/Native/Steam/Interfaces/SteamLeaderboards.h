#pragma once

#ifdef USE_STEAMWORKS

#include "CoreMinimal.h"
#include "Native/Interfaces/ILeaderboardsService.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/** Steam leaderboard write + paged query via the global SteamUserStats() + the core CallResults pump. */
	class FSteamLeaderboards final : public ILeaderboardsService
	{
	public:
		explicit FSteamLeaderboards(FSteamPlatformCore& InCore) : Core(InCore) {}

		virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
		                                   TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 Offset,
		                                  TFunction<void(const FLeaderboardResult&)> Callback) override;
		virtual void QueryLeaderboardUserRank(const FString& LeaderboardId,
		                                      TFunction<void(const FLeaderboardResult&)> Callback) override;

	private:
		// Leaderboard/API-call handles are SDK uint64 typedefs (SteamLeaderboard_t); the header stays
		// SDK-free by passing them as uint64, the .cpp uses them directly against the Steam interface.
		void HandleUploadLeaderboardScore(uint64 Leaderboard, int32 Score,
		                                  TFunction<void(const FGamingServiceResult&)> Callback);
		void HandleDownloadLeaderboardEntries(uint64 Leaderboard, const FString& LeaderboardId, int32 Limit,
		                                      int32 Offset, TFunction<void(const FLeaderboardResult&)> Callback);
		// Downloads the caller's own around-user entry to fill UserRank/UserScore (Entries stays empty).
		void HandleDownloadUserRank(uint64 Leaderboard, const FString& LeaderboardId,
		                            TFunction<void(const FLeaderboardResult&)> Callback);

		FSteamPlatformCore& Core;
	};
}

#endif // USE_STEAMWORKS

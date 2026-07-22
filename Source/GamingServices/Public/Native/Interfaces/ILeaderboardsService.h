#pragma once

#include "CoreMinimal.h"
#include "DataTypes/LeaderboardTypes.h"
#include "Native/GamingCapability.h"

/** Leaderboard write + random-access paged read + self-rank capability. */
class GAMINGSERVICES_API ILeaderboardsService
{
public:
	virtual ~ILeaderboardsService() = default;

	virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                                   TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	/**
	 * Read a page of the board. Offset is a 0-based start index into the rank-sorted board (random
	 * access: page N of size L is Offset = N*L), Limit is the page size. The result's TotalEntries is
	 * the full board size and NextOffset is the offset to pass for the following page (-1 when the end
	 * is reached). UserRank/UserScore are NOT populated here — use QueryLeaderboardUserRank for that.
	 */
	virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 Offset,
	                                  TFunction<void(const FLeaderboardResult&)> Callback) = 0;

	/**
	 * Find the querying user's own position on the board without paging to it — the way to locate
	 * yourself on a board too large to walk (e.g. a shared global board). The result's UserRank
	 * (1-based) and UserScore are filled in; Entries is empty and UserRank is -1 if the user has no
	 * entry. Steam uses an around-user query; EOS reads the user's record from the downloaded board.
	 */
	virtual void QueryLeaderboardUserRank(const FString& LeaderboardId,
	                                      TFunction<void(const FLeaderboardResult&)> Callback) = 0;
};

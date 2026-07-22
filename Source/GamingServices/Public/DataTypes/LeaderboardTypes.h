#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "LeaderboardTypes.generated.h"

USTRUCT(BlueprintType)
struct FLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString UserId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Rank = 0;

	FLeaderboardEntry() = default;

	FLeaderboardEntry(const FString& InUserId, const FString& InDisplayName, int32 InScore, int32 InRank)
		: UserId(InUserId), DisplayName(InDisplayName), Score(InScore), Rank(InRank)
	{
	}
};

USTRUCT(BlueprintType)
struct FLeaderboardResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString LeaderboardId;

	UPROPERTY(BlueprintReadOnly)
	TArray<FLeaderboardEntry> Entries;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalEntries = 0;

	/** 0-based start index to pass as Offset for the next page, or -1 when this was the last page. */
	UPROPERTY(BlueprintReadOnly)
	int32 NextOffset = -1;

	/** The querying user's own rank (1-based) and score, or -1 rank when they have no entry. Populated
	 *  by QueryLeaderboardUserRank; left at the defaults by the paged query. */
	UPROPERTY(BlueprintReadOnly)
	int32 UserRank = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 UserScore = 0;

	FLeaderboardResult() = default;

	FLeaderboardResult(bool InSuccess, const FString& InLeaderboardId = TEXT(""),
	                   const TArray<FLeaderboardEntry>& InEntries = TArray<FLeaderboardEntry>(),
	                   int32 InTotalEntries = 0, int32 InUserRank = -1, int32 InUserScore = 0)
		: FGamingServiceResult(InSuccess)
		  , LeaderboardId(InLeaderboardId), Entries(InEntries), TotalEntries(InTotalEntries)
		  , UserRank(InUserRank), UserScore(InUserScore)
	{
	}
};

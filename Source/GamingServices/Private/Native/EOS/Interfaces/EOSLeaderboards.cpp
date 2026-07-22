#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSLeaderboards.h"
#include "Native/EOS/Interfaces/EOSStats.h"
#include "EOSCommon.h"
#include "EOSCallbackContext.h"

#include <string>

namespace GamingServices
{
	using FLeaderboardCallbackCtx = TEOSCallbackContext<FLeaderboardResult, FEOSLeaderboards>;

	namespace
	{
		// Cast the core's opaque accessor back to its EOS_* type here so the core header stays SDK-free.
		EOS_HLeaderboards LeaderboardsHandle(const FEOSPlatformCore& Core)
		{
			return static_cast<EOS_HLeaderboards>(Core.GetLeaderboardsHandle());
		}

		// Map an EOS leaderboard record onto the game's leaderboard entry.
		void ConvertEOSLeaderboardRecordToEntry(const EOS_Leaderboards_LeaderboardRecord* EOSRecord,
		                                        FLeaderboardEntry& Entry)
		{
			if (EOSRecord)
			{
				// Record->UserId is an EOS_ProductUserId handle, not a string; stringify it properly.
				char PuidStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
				int32_t PuidLen = sizeof(PuidStr);
				if (EOS_ProductUserId_ToString(EOSRecord->UserId, PuidStr, &PuidLen) == EOS_EResult::EOS_Success)
				{
					Entry.UserId = UTF8_TO_TCHAR(PuidStr);
				}
				Entry.DisplayName = UTF8_TO_TCHAR(EOSRecord->UserDisplayName);
				Entry.Score = EOSRecord->Score;
				Entry.Rank = EOSRecord->Rank;
			}
		}
	}

	void FEOSLeaderboards::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                                             TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetStatsHandle(),
		       TEXT("EOSGamingService: WriteLeaderboardScore called when service not ready"));
		checkf(Core.AreDefinitionsLoaded(),
		       TEXT("EOSGamingService: WriteLeaderboardScore called before definitions are loaded"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Writing leaderboard score: %d to %s"), Score, *LeaderboardId);

		const EOS_Leaderboards_Definition* LeaderboardDef =
			static_cast<const EOS_Leaderboards_Definition*>(Core.FindLeaderboardDefinition(LeaderboardId));

		if (!LeaderboardDef)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Leaderboard definition not found for: %s"), *LeaderboardId);
			Callback(FGamingServiceResult(false));
			return;
		}

		FString StatName = UTF8_TO_TCHAR(LeaderboardDef->StatName);
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Found stat name '%s' for leaderboard '%s'"), *StatName,
		       *LeaderboardId);

		Stats.IngestStat(StatName, Score, [Callback](const FGamingServiceResult& StatResult)
		{
			Callback(StatResult);
		});
	}

	void FEOSLeaderboards::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 Offset,
	                                            TFunction<void(const FLeaderboardResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLeaderboardsHandle(),
		       TEXT("EOSGamingService: QueryLeaderboardPage called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Querying leaderboard page: %s (Offset: %d, Limit: %d)"),
		       *LeaderboardId, Offset, Limit);

		// Must outlive the EOS_Leaderboards_QueryLeaderboardRanks call below; assigning TCHAR_TO_UTF8()
		// directly leaves a dangling pointer.
		const std::string LeaderboardIdUtf8 = TCHAR_TO_UTF8(*LeaderboardId);

		EOS_Leaderboards_QueryLeaderboardRanksOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST;
		QueryOptions.LeaderboardId = LeaderboardIdUtf8.c_str();
		QueryOptions.LocalUserId = ProductUserId(Core);

		struct FLeaderboardQueryCtx : FLeaderboardCallbackCtx
		{
			FString LeaderboardId;
			int32 Limit;
			int32 Offset;
		};
		auto* Ctx = new FLeaderboardQueryCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->LeaderboardId = LeaderboardId;
		Ctx->Limit = Limit;
		Ctx->Offset = Offset;

		EOS_Leaderboards_QueryLeaderboardRanks(
			LeaderboardsHandle(Core),
			&QueryOptions,
			Ctx,
			[](const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FLeaderboardQueryCtx*>(Data->ClientData);
				FEOSLeaderboards* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);
				FLeaderboardResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				Result.LeaderboardId = LocalCtx->LeaderboardId;
				if (!Result.bSuccess)
				{
					FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
					return;
				}
				EOS_Leaderboards_GetLeaderboardRecordCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST;
				uint32_t RecordCount = EOS_Leaderboards_GetLeaderboardRecordCount(
					LeaderboardsHandle(Self->Core), &CountOptions);

				// EOS downloads the whole board (no server-side range), so Offset is a client-side
				// start index into the rank-sorted records — copy just the requested window.
				TArray<FLeaderboardEntry> Entries;
				const uint32_t StartIndex = FMath::Max(0, LocalCtx->Offset);
				const uint32_t EndIndex = FMath::Min(RecordCount, StartIndex + LocalCtx->Limit);
				for (uint32_t i = StartIndex; i < EndIndex; ++i)
				{
					EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST;
					CopyOptions.LeaderboardRecordIndex = i;

					EOS_Leaderboards_LeaderboardRecord* Record = nullptr;
					if (EOS_Leaderboards_CopyLeaderboardRecordByIndex(
						LeaderboardsHandle(Self->Core), &CopyOptions, &Record) == EOS_EResult::EOS_Success && Record)
					{
						FLeaderboardEntry Entry;
						ConvertEOSLeaderboardRecordToEntry(Record, Entry);
						Entries.Add(Entry);
						EOS_Leaderboards_LeaderboardRecord_Release(Record);
					}
				}
				Result.Entries = Entries;
				Result.TotalEntries = RecordCount;
				Result.NextOffset = EndIndex < RecordCount ? EndIndex : -1;
				FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void FEOSLeaderboards::QueryLeaderboardUserRank(const FString& LeaderboardId,
	                                                TFunction<void(const FLeaderboardResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLeaderboardsHandle(),
		       TEXT("EOSGamingService: QueryLeaderboardUserRank called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Querying own rank on leaderboard: %s"), *LeaderboardId);

		const std::string LeaderboardIdUtf8 = TCHAR_TO_UTF8(*LeaderboardId);

		EOS_Leaderboards_QueryLeaderboardRanksOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST;
		QueryOptions.LeaderboardId = LeaderboardIdUtf8.c_str();
		QueryOptions.LocalUserId = ProductUserId(Core);

		struct FUserRankCtx : FLeaderboardCallbackCtx
		{
			FString LeaderboardId;
		};
		auto* Ctx = new FUserRankCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->LeaderboardId = LeaderboardId;

		EOS_Leaderboards_QueryLeaderboardRanks(
			LeaderboardsHandle(Core),
			&QueryOptions,
			Ctx,
			[](const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FUserRankCtx*>(Data->ClientData);
				FEOSLeaderboards* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);
				FLeaderboardResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				Result.LeaderboardId = LocalCtx->LeaderboardId;
				if (!Result.bSuccess)
				{
					FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
					return;
				}
				EOS_Leaderboards_GetLeaderboardRecordCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST;
				uint32_t RecordCount = EOS_Leaderboards_GetLeaderboardRecordCount(
					LeaderboardsHandle(Self->Core), &CountOptions);
				Result.TotalEntries = RecordCount;

				// The board is rank-sorted and fully downloaded; scan for the caller's own record.
				const FString SelfUserId = Self->Core.GetUserId();
				for (uint32_t i = 0; i < RecordCount; ++i)
				{
					EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST;
					CopyOptions.LeaderboardRecordIndex = i;

					EOS_Leaderboards_LeaderboardRecord* Record = nullptr;
					if (EOS_Leaderboards_CopyLeaderboardRecordByIndex(
						LeaderboardsHandle(Self->Core), &CopyOptions, &Record) == EOS_EResult::EOS_Success && Record)
					{
						FLeaderboardEntry Entry;
						ConvertEOSLeaderboardRecordToEntry(Record, Entry);
						EOS_Leaderboards_LeaderboardRecord_Release(Record);
						if (Entry.UserId == SelfUserId)
						{
							Result.UserRank = Entry.Rank;
							Result.UserScore = Entry.Score;
							break;
						}
					}
				}
				FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}
}

#endif // USE_EOS

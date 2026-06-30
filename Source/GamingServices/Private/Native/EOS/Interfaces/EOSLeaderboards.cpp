#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSLeaderboards.h"
#include "Native/EOS/Interfaces/EOSStats.h"
#include "Native/EOS/EOSPlatformCore.h"
#include "EOSCallbackContext.h"

namespace GamingServices
{
	using FLeaderboardCallbackCtx = TEOSCallbackContext<FLeaderboardResult, FEOSLeaderboards>;

	// Cast the core's opaque accessors back to their EOS_* types in this .cpp so the core header stays SDK-free.
	static EOS_HLeaderboards LeaderboardsHandle(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_HLeaderboards>(Core.GetLeaderboardsHandle());
	}

	static EOS_ProductUserId ProductUserId(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_ProductUserId>(Core.GetProductUserId());
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

	void FEOSLeaderboards::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
	                                            TFunction<void(const FLeaderboardResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLeaderboardsHandle(),
		       TEXT("EOSGamingService: QueryLeaderboardPage called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Querying leaderboard page: %s (Continuation: %d, Limit: %d)"),
		       *LeaderboardId, ContinuationToken, Limit);

		EOS_Leaderboards_QueryLeaderboardRanksOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST;
		QueryOptions.LeaderboardId = TCHAR_TO_UTF8(*LeaderboardId);
		QueryOptions.LocalUserId = ProductUserId(Core);

		struct FLeaderboardQueryCtx : FLeaderboardCallbackCtx
		{
			FString LeaderboardId;
			int32 Limit;
			int32 ContinuationToken;
		};
		auto* Ctx = new FLeaderboardQueryCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->LeaderboardId = LeaderboardId;
		Ctx->Limit = Limit;
		Ctx->ContinuationToken = ContinuationToken;

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

				TArray<FLeaderboardEntry> Entries;
				uint32_t StartIndex = FMath::Max(0, LocalCtx->ContinuationToken);
				uint32_t EndIndex = FMath::Min(RecordCount, StartIndex + LocalCtx->Limit);
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
				Result.ContinuationToken = EndIndex < RecordCount ? EndIndex : -1;
				FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}
}

#endif // USE_EOS

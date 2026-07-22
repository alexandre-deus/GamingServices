#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSStats.h"
#include "EOSCommon.h"
#include "EOSCallbackContext.h"

#include <string>

namespace GamingServices
{
	using FStatIngestCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSStats>;
	using FStatQueryCallbackCtx = TEOSCallbackContext<FStatQueryResult, FEOSStats>;

	// Cast the core's opaque accessors back to their EOS_* types in this .cpp so the core header stays SDK-free.
	static EOS_HStats StatsHandle(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_HStats>(Core.GetStatsHandle());
	}

	void FEOSStats::IngestStat(const FString& StatName, int32 Amount,
	                           TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetStatsHandle(),
		       TEXT("EOSGamingService: IngestStat called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Ingesting %d into stat %s"), Amount, *StatName);

		EOS_Stats_IngestStatOptions IngestOptions = {};
		IngestOptions.ApiVersion = EOS_STATS_INGESTSTAT_API_LATEST;
		IngestOptions.LocalUserId = ProductUserId(Core);
		IngestOptions.TargetUserId = ProductUserId(Core);
		IngestOptions.StatsCount = 1;

		// Must outlive the EOS_Stats_IngestStat call below; assigning TCHAR_TO_UTF8() directly leaves
		// a dangling pointer.
		const std::string StatNameUtf8 = TCHAR_TO_UTF8(*StatName);

		EOS_Stats_IngestData Stat = {};
		Stat.ApiVersion = EOS_STATS_INGESTDATA_API_LATEST;
		Stat.StatName = StatNameUtf8.c_str();
		Stat.IngestAmount = Amount;

		IngestOptions.Stats = &Stat;

		struct FStatIngestCtx : FStatIngestCallbackCtx
		{
			FString StatName;
			int32 Amount = 0;
		};
		auto* Ctx = new FStatIngestCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->StatName = StatName;
		Ctx->Amount = Amount;

		EOS_Stats_IngestStat(
			StatsHandle(Core),
			&IngestOptions,
			Ctx,
			[](const EOS_Stats_IngestStatCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FStatIngestCtx*>(Data->ClientData);
				FGamingServiceResult Result((Data->ResultCode == EOS_EResult::EOS_Success));
				FStatIngestCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void FEOSStats::QueryStat(const FString& StatName,
	                          TFunction<void(const FStatQueryResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetStatsHandle() && Core.GetProductUserId() != nullptr,
		       TEXT("EOSGamingService: QueryStat called when service not ready"));
		// (Core.Get*Handle()/GetProductUserId() return void*; cast via the local helpers when passing to the SDK.)

		struct FStatQueryCtx : FStatQueryCallbackCtx
		{
			FString StatName;
		};
		auto* Ctx = new FStatQueryCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->StatName = StatName;

		// Query the requested stat by name: EOS returns an empty set when StatNames is omitted,
		// so the previous query-everything approach never found anything.
		const std::string StatNameUtf8 = TCHAR_TO_UTF8(*StatName);
		const char* StatNames[] = {StatNameUtf8.c_str()};

		EOS_Stats_QueryStatsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_STATS_QUERYSTATS_API_LATEST;
		QueryOptions.LocalUserId = ProductUserId(Core);
		QueryOptions.TargetUserId = ProductUserId(Core);
		QueryOptions.StatNames = StatNames;
		QueryOptions.StatNamesCount = 1;
		QueryOptions.StartTime = EOS_STATS_TIME_UNDEFINED;
		QueryOptions.EndTime = EOS_STATS_TIME_UNDEFINED;

		EOS_Stats_QueryStats(
			StatsHandle(Core),
			&QueryOptions,
			Ctx,
			[](const EOS_Stats_OnQueryStatsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FStatQueryCtx*>(Data->ClientData);
				FEOSStats* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					FStatQueryResult Result;
					Result.bSuccess = false;
					Result.StatName = LocalCtx->StatName;
					FStatQueryCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				EOS_Stats_GetStatCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_STATS_GETSTATCOUNT_API_LATEST;
				CountOptions.TargetUserId = ProductUserId(Self->Core);
				uint32_t Count = EOS_Stats_GetStatsCount(StatsHandle(Self->Core), &CountOptions);

				int32 FoundValue = 0;
				bool bFound = false;
				for (uint32_t i = 0; i < Count; ++i)
				{
					EOS_Stats_CopyStatByIndexOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_STATS_COPYSTATBYINDEX_API_LATEST;
					CopyOptions.TargetUserId = ProductUserId(Self->Core);
					CopyOptions.StatIndex = i;

					EOS_Stats_Stat* Stat = nullptr;
					if (EOS_Stats_CopyStatByIndex(StatsHandle(Self->Core), &CopyOptions, &Stat) ==
						EOS_EResult::EOS_Success && Stat)
					{
						if (LocalCtx->StatName == UTF8_TO_TCHAR(Stat->Name))
						{
							FoundValue = Stat->Value;
							bFound = true;
						}
						EOS_Stats_Stat_Release(Stat);
					}
				}

				FStatQueryResult Result;
				if (bFound)
				{
					Result = FStatQueryResult::Make(LocalCtx->StatName, FoundValue);
				}
				else
				{
					Result.bSuccess = false;
					Result.StatName = LocalCtx->StatName;
				}
				FStatQueryCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}
}

#endif // USE_EOS

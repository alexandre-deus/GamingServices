#ifdef USE_STEAMWORKS

#include "Native/Steam/Interfaces/SteamStats.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	void FSteamStats::IngestStat(const FString& StatName, int32 Amount, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: IngestStat called when service not "
					"ready"));

		FTCHARToUTF8 StatNameUTF8(*StatName);
		const char* StatId = StatNameUTF8.Get();

		int32 CurrentInt = 0;
		bool bHasInt = SteamUserStats->GetStat(StatId, &CurrentInt);
		if (bHasInt)
		{
			SteamUserStats->SetStat(StatId, CurrentInt + Amount);
			SteamUserStats->StoreStats();
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
			return;
		}

		float CurrentFloat = 0.0f;
		bool bHasFloat = SteamUserStats->GetStat(StatId, &CurrentFloat);
		if (bHasFloat)
		{
			SteamUserStats->SetStat(StatId, CurrentFloat + static_cast<float>(Amount));
			SteamUserStats->StoreStats();
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Stat not found: %s"), *StatName);
		if (Callback)
		{
			Callback(FGamingServiceResult(false));
		}
	}

	void FSteamStats::QueryStat(const FString& StatName, TFunction<void(const FStatQueryResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: QueryStat called when service not "
					"ready"));
		FTCHARToUTF8 NameUTF8(*StatName);
		const char* StatId = NameUTF8.Get();
		int32 Value = 0;
		if (SteamUserStats->GetStat(StatId, &Value))
		{
			if (Callback)
			{
				Callback(FStatQueryResult::Make(StatName, Value));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Stat not found: %s"), *StatName);
			FStatQueryResult Result;
			Result.bSuccess = false;
			Result.StatName = StatName;
			if (Callback)
			{
				Callback(Result);
			}
		}
	}
}

#endif // USE_STEAMWORKS

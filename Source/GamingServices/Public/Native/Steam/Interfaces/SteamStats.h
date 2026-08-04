#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IStatsService.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/** Steam stat ingest + query via the global SteamUserStats() interface. */
	class FSteamStats final : public IStatsService
	{
	public:
		explicit FSteamStats(FSteamPlatformCore& InCore) : Core(InCore) {}

		virtual void IngestStat(const FString& StatName, int32 Amount,
		                        TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryStat(const FString& StatName,
		                       TFunction<void(const FStatQueryResult&)> Callback) override;

	private:
		FSteamPlatformCore& Core;
	};
}

#endif // GS_WITH_STEAM

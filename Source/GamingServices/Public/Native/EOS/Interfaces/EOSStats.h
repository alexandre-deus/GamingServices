#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IStatsService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/** EOS stats capability (ingest + query) over the shared platform core. */
	class FEOSStats final : public IStatsService
	{
	public:
		explicit FEOSStats(FEOSPlatformCore& InCore) : Core(InCore) {}

		virtual void IngestStat(const FString& StatName, int32 Amount,
		                        TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryStat(const FString& StatName,
		                       TFunction<void(const FStatQueryResult&)> Callback) override;

	private:
		FEOSPlatformCore& Core;
	};
}

#endif // USE_EOS

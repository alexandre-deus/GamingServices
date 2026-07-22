#pragma once

#include "CoreMinimal.h"
#include "DataTypes/StatTypes.h"
#include "Native/GamingCapability.h"

/** Platform stat ingest + query capability. */
class GAMINGSERVICES_API IStatsService
{
public:
	virtual ~IStatsService() = default;

	virtual void IngestStat(const FString& StatName, int32 Amount,
	                        TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryStat(const FString& StatName,
	                       TFunction<void(const FStatQueryResult&)> Callback) = 0;
};

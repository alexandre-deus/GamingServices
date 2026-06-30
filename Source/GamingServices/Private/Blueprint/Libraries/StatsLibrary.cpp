#include "Blueprint/Libraries/StatsLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IStatsService.h"

UAsyncAction_IngestStat* UAsyncAction_IngestStat::IngestStat(UObject* WorldContextObject, const FString& StatName, int32 Amount)
{
	UAsyncAction_IngestStat* Action = NewObject<UAsyncAction_IngestStat>();
	Action->WorldContext = WorldContextObject;
	Action->StatName = StatName;
	Action->Amount = Amount;
	return Action;
}

void UAsyncAction_IngestStat::Activate()
{
	IGamingService* Service = ResolveService();
	IStatsService* Stats = Service ? Service->GetStats() : nullptr;
	if (!Stats)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Stats->IngestStat(StatName, Amount, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_QueryStat* UAsyncAction_QueryStat::QueryStat(UObject* WorldContextObject, const FString& StatName)
{
	UAsyncAction_QueryStat* Action = NewObject<UAsyncAction_QueryStat>();
	Action->WorldContext = WorldContextObject;
	Action->StatName = StatName;
	return Action;
}

void UAsyncAction_QueryStat::Activate()
{
	IGamingService* Service = ResolveService();
	IStatsService* Stats = Service ? Service->GetStats() : nullptr;
	if (!Stats)
	{
		Completed.Broadcast(FStatQueryResult());
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Stats->QueryStat(StatName, [this](const FStatQueryResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

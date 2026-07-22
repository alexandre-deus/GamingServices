#include "Blueprint/Libraries/LeaderboardsLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/ILeaderboardsService.h"

UAsyncAction_WriteLeaderboardScore* UAsyncAction_WriteLeaderboardScore::WriteLeaderboardScore(UObject* WorldContextObject, const FString& LeaderboardId, int32 Score)
{
	UAsyncAction_WriteLeaderboardScore* Action = NewObject<UAsyncAction_WriteLeaderboardScore>();
	Action->WorldContext = WorldContextObject;
	Action->LeaderboardId = LeaderboardId;
	Action->Score = Score;
	return Action;
}

void UAsyncAction_WriteLeaderboardScore::Activate()
{
	IGamingService* Service = ResolveService();
	ILeaderboardsService* Leaderboards = Service ? Service->GetLeaderboards() : nullptr;
	if (!Leaderboards)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Leaderboards->WriteLeaderboardScore(LeaderboardId, Score, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_QueryLeaderboardPage* UAsyncAction_QueryLeaderboardPage::QueryLeaderboardPage(UObject* WorldContextObject, const FString& LeaderboardId, int32 Limit, int32 Offset)
{
	UAsyncAction_QueryLeaderboardPage* Action = NewObject<UAsyncAction_QueryLeaderboardPage>();
	Action->WorldContext = WorldContextObject;
	Action->LeaderboardId = LeaderboardId;
	Action->Limit = Limit;
	Action->Offset = Offset;
	return Action;
}

void UAsyncAction_QueryLeaderboardPage::Activate()
{
	IGamingService* Service = ResolveService();
	ILeaderboardsService* Leaderboards = Service ? Service->GetLeaderboards() : nullptr;
	if (!Leaderboards)
	{
		Completed.Broadcast(FLeaderboardResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Leaderboards->QueryLeaderboardPage(LeaderboardId, Limit, Offset, [this](const FLeaderboardResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_QueryLeaderboardUserRank* UAsyncAction_QueryLeaderboardUserRank::QueryLeaderboardUserRank(UObject* WorldContextObject, const FString& LeaderboardId)
{
	UAsyncAction_QueryLeaderboardUserRank* Action = NewObject<UAsyncAction_QueryLeaderboardUserRank>();
	Action->WorldContext = WorldContextObject;
	Action->LeaderboardId = LeaderboardId;
	return Action;
}

void UAsyncAction_QueryLeaderboardUserRank::Activate()
{
	IGamingService* Service = ResolveService();
	ILeaderboardsService* Leaderboards = Service ? Service->GetLeaderboards() : nullptr;
	if (!Leaderboards)
	{
		Completed.Broadcast(FLeaderboardResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Leaderboards->QueryLeaderboardUserRank(LeaderboardId, [this](const FLeaderboardResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

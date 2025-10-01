#include "NullGamingService.h"
#include "GamingServiceTypes.h"

FNullGamingService::FNullGamingService()
	: bInitialized(false)
	, bLoggedIn(false)
{
}

FNullGamingService::~FNullGamingService()
{
}

bool FNullGamingService::Connect(const FGamingServiceConnectParams& Params)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: Connect called - no SDK available"));
	bInitialized = true;
	return true;
}

void FNullGamingService::Shutdown()
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: Shutdown called"));
	bInitialized = false;
	bLoggedIn = false;
}

void FNullGamingService::Login(const FGamingServiceLoginParams& Params,
                               TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: Login called - no SDK available"));
	
	FGamingServiceResult Result;
	Result.bSuccess = true;
	
	bLoggedIn = true;

	Callback(Result);
}

void FNullGamingService::UnlockAchievement(const FString& AchievementId,
                                           TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: UnlockAchievement called for %s - no SDK available"), *AchievementId);
	
	FGamingServiceResult Result;
	Result.bSuccess = true;
	
	Callback(Result);
}

void FNullGamingService::QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: QueryAchievements called - no SDK available"));
	
	FAchievementsQueryResult Result;
	Result.bSuccess = true;
	
	Callback(Result);
}

void FNullGamingService::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
                                               TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: WriteLeaderboardScore called for %s with score %d - no SDK available"), *LeaderboardId, Score);
	
	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
                                              TFunction<void(const FLeaderboardResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: QueryLeaderboardPage called for %s - no SDK available"), *LeaderboardId);
	
	FLeaderboardResult Result;
	Result.bSuccess = true;
	
	Callback(Result);
}

void FNullGamingService::IngestStat(const FString& StatName, int32 Amount,
                                    TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: IngestStat called for %s with amount %d - no SDK available"), *StatName, Amount);
	
	FGamingServiceResult Result;
	Result.bSuccess = true;
	
	Callback(Result);
}

void FNullGamingService::QueryStat(const FString& StatName,
                                   TFunction<void(const FStatQueryResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: QueryStat called for %s - no SDK available"), *StatName);
	
	FStatQueryResult Result;
	Result.bSuccess = true;
	
	Callback(Result);
}

void FNullGamingService::Tick()
{
	// No-op for null service
}

bool FNullGamingService::IsInitialized() const
{
	return bInitialized;
}

bool FNullGamingService::IsLoggedIn() const
{
	return bLoggedIn;
}

bool FNullGamingService::NeedsLogin() const
{
	return !bLoggedIn;
}

FString FNullGamingService::GetUserId() const
{
	return TEXT("NullUser");
}

FString FNullGamingService::GetDisplayName() const
{
	return TEXT("Null User");
}

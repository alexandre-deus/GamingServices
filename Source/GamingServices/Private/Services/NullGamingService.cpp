#include "Services/NullGamingService.h"
#include "GamingServiceTypes.h"
#include "Misc/Paths.h"

FNullGamingService::FNullGamingService()
	: bInitialized(false)
	, bLoggedIn(false)
{
}

FNullGamingService::~FNullGamingService()
{
}

void FNullGamingService::InitializePlatform()
{
	bInitialized = true;
}

void FNullGamingService::DestroyPlatform()
{
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

void FNullGamingService::ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: ListEntitlements called - no SDK available"));

	FEntitlementsListResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::HasEntitlement(const FEntitlementDefinition& Definition,
                                        TFunction<void(const FHasEntitlementResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: HasEntitlement called for '%s' - no SDK available"),
	       *Definition.LogicalName.ToString());

	FHasEntitlementResult Result(true, Definition.LogicalName.ToString(), false);

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

void FNullGamingService::WriteFile(const FString& FilePath, const TArray<uint8>& Data,
                                   TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: WriteFile called for %s (%d bytes) - no SDK available"), *FilePath, Data.Num());
	
	FGamingServiceResult Result;
	Result.bSuccess = true;
	
	Callback(Result);
}

void FNullGamingService::ReadFile(const FString& FilePath,
                                  TFunction<void(const FFileReadResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: ReadFile called for %s - no SDK available"), *FilePath);

	Callback(FFileReadResult(true, FilePath, TArray<uint8>()));
}

void FNullGamingService::DeleteFile(const FString& FilePath,
                                    TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: DeleteFile called for %s - no SDK available"), *FilePath);

	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::ListFiles(const FString& DirectoryPath,
                                   TFunction<void(const FFilesListResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: ListFiles called for %s - no SDK available"), *DirectoryPath);

	FFilesListResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::CreateSession(const FSessionSettings& Settings,
                                       TFunction<void(const FSessionCreateResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: CreateSession called for %s - no SDK available"), *Settings.SessionName);

	FSessionCreateResult Result;
	Result.bSuccess = true;
	Result.SessionInfo.SessionName = Settings.SessionName;

	Callback(Result);
}

void FNullGamingService::FindSessions(const FSessionSearchFilter& Filter,
                                      TFunction<void(const FSessionSearchResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: FindSessions called - no SDK available"));

	FSessionSearchResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::JoinSession(const FSessionJoinHandle& JoinHandle,
                                     TFunction<void(const FSessionJoinResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: JoinSession called - no SDK available"));

	FSessionJoinResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: LeaveSession called - no SDK available"));

	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: DestroySession called - no SDK available"));

	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::UpdateSession(const FSessionSettings& Settings,
                                       TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: UpdateSession called for %s - no SDK available"), *Settings.SessionName);

	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::LockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: LockLobby called - no SDK available"));

	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: UnlockLobby called - no SDK available"));

	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

void FNullGamingService::GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: GetCurrentSession called - no SDK available"));

	FSessionInfo Info;

	Callback(Info);
}

void FNullGamingService::ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback)
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: ShowInviteFriendsDialog called - no SDK available"));

	FGamingServiceResult Result;
	Result.bSuccess = true;

	Callback(Result);
}

FString FNullGamingService::GetSessionConnectionString() const
{
	UE_LOG(LogTemp, Warning, TEXT("NullGamingService: GetSessionConnectionString called - no SDK available"));
	return FString();
}

void FNullGamingService::Tick()
{
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

UTexture2D* FNullGamingService::GetAvatar() const
{
	return nullptr;
}

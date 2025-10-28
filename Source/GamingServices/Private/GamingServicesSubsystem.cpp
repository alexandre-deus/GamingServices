#include "GamingServicesSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "GamingServiceTypes.h"
#include "EOSGamingService.h"
#include "SteamworksGamingService.h"
#include "NullGamingService.h"

void UGamingServicesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
#ifdef USE_EOS
	Service = MakeUnique<FEOSGamingService>();
#elif defined(USE_STEAMWORKS)
    Service = MakeUnique<FSteamworksGamingService>();
#else
	Service = MakeUnique<FNullGamingService>();
#endif
	check(Service);
	Super::Initialize(Collection);
}

void UGamingServicesSubsystem::Deinitialize()
{
	Service->Shutdown();
}

void UGamingServicesSubsystem::Tick(float DeltaTime)
{
	Service->Tick();
}

TStatId UGamingServicesSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGamingServicesSubsystem, STATGROUP_Tickables);
}

bool UGamingServicesSubsystem::Connect(const FGamingServiceConnectParams& Params)
{
	return Service->Connect(Params);
}

void UGamingServicesSubsystem::Shutdown()
{
	Service->Shutdown();
}

void UGamingServicesSubsystem::UnlockAchievement(const FString& AchievementId)
{
	Service->UnlockAchievement(AchievementId, [this](const FGamingServiceResult& R)
	{
		OnAchievementUnlocked.Broadcast(R);
	});
}

void UGamingServicesSubsystem::QueryAchievements()
{
	Service->QueryAchievements([this](const FAchievementsQueryResult& R)
	{
		OnAchievementsQueried.Broadcast(R);
	});
}

void UGamingServicesSubsystem::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score)
{
	Service->WriteLeaderboardScore(LeaderboardId, Score, [this](const FGamingServiceResult& R)
	{
		OnLeaderboardScoreWritten.Broadcast(R);
	});
}

void UGamingServicesSubsystem::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken)
{
	Service->QueryLeaderboardPage(LeaderboardId, Limit, ContinuationToken, [this](const FLeaderboardResult& R)
	{
		OnLeaderboardQueried.Broadcast(R);
	});
}

void UGamingServicesSubsystem::IngestStat(const FString& StatName, int32 Amount)
{
	Service->IngestStat(StatName, Amount, [this](const FGamingServiceResult& R)
	{
		OnStatProgressed.Broadcast(R);
	});
}

void UGamingServicesSubsystem::QueryStat(const FString& StatName)
{
	Service->QueryStat(StatName, [this](const FStatQueryResult& R)
	{
		OnStatQueried.Broadcast(R);
	});
}

void UGamingServicesSubsystem::Login(const FGamingServiceLoginParams& Params)
{
	Service->Login(Params, [this](const FGamingServiceResult& Result)
	{
		OnLoggedIn.Broadcast(Result);
	});
}

bool UGamingServicesSubsystem::IsConnected() const
{
	return Service->IsInitialized();
}

bool UGamingServicesSubsystem::IsLoggedIn() const
{
	return Service->IsLoggedIn();
}

bool UGamingServicesSubsystem::NeedsLogin() const
{
	return Service->NeedsLogin();
}

void UGamingServicesSubsystem::WriteFile(const FString& FilePath, const TArray<uint8>& Data)
{
	Service->WriteFile(FilePath, Data, [this](const FGamingServiceResult& R)
	{
		OnFileWritten.Broadcast(R);
	});
}

void UGamingServicesSubsystem::ReadFile(const FString& FilePath)
{
	Service->ReadFile(FilePath, [this](const FFileReadResult& R)
	{
		OnFileRead.Broadcast(R);
	});
}

void UGamingServicesSubsystem::DeleteFile(const FString& FilePath)
{
	Service->DeleteFile(FilePath, [this](const FGamingServiceResult& R)
	{
		OnFileDeleted.Broadcast(R);
	});
}

void UGamingServicesSubsystem::ListFiles(const FString& DirectoryPath)
{
	Service->ListFiles(DirectoryPath, [this](const FFilesListResult& R)
	{
		OnFilesListed.Broadcast(R);
	});
}

void UGamingServicesSubsystem::SetRemoteSetting(const FString& Key, const FString& Value)
{
	Service->SetRemoteSetting(Key, Value, [this](const FRemoteSettingResult& R)
	{
		OnRemoteSettingChanged.Broadcast(R);
	});
}

void UGamingServicesSubsystem::GetRemoteSetting(const FString& Key)
{
	Service->GetRemoteSetting(Key, [this](const FRemoteSettingResult& R)
	{
		OnRemoteSettingQueried.Broadcast(R);
	});
}

void UGamingServicesSubsystem::DeleteRemoteSetting(const FString& Key)
{
	Service->DeleteRemoteSetting(Key, [this](const FRemoteSettingResult& R)
	{
		OnRemoteSettingDeleted.Broadcast(R);
	});
}

void UGamingServicesSubsystem::ListRemoteSettings()
{
	Service->ListRemoteSettings([this](const FRemoteSettingsListResult& R)
	{
		OnRemoteSettingsListed.Broadcast(R);
	});
}
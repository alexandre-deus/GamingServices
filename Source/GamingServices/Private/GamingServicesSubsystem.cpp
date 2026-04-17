#include "GamingServicesSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "GamingServiceTypes.h"
#include "Services/EOSGamingService.h"
#include "Services/SteamworksGamingService.h"
#include "Services/NullGamingService.h"

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

	// Wire up session event callbacks from the service to subsystem delegates
	Service->OnSessionUserJoined = [this](const FSessionMemberInfo& MemberInfo)
	{
		OnSessionUserJoined.Broadcast(MemberInfo);
	};
	Service->OnSessionUserLeft = [this](const FSessionMemberInfo& MemberInfo)
	{
		OnSessionUserLeft.Broadcast(MemberInfo);
	};
	Service->OnLobbyInviteAccepted = [this](const FLobbyInviteAcceptedInfo& InviteInfo)
	{
		OnLobbyInviteAccepted.Broadcast(InviteInfo);
	};

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

void UGamingServicesSubsystem::RegisterEntitlement(const FEntitlementDefinition& Definition)
{
	EntitlementCatalog.Add(Definition.LogicalName, Definition);
}

void UGamingServicesSubsystem::ListEntitlements()
{
	Service->ListEntitlements([this](const FEntitlementsListResult& R)
	{
		OnEntitlementsListed.Broadcast(R);
	});
}

void UGamingServicesSubsystem::HasEntitlement(FName LogicalName)
{
	const FEntitlementDefinition* Def = EntitlementCatalog.Find(LogicalName);
	if (!Def)
	{
		UE_LOG(LogTemp, Error, TEXT("GamingServices: Entitlement '%s' not registered"), *LogicalName.ToString());
		OnEntitlementChecked.Broadcast(FHasEntitlementResult(false, LogicalName.ToString(), false));
		return;
	}
	Service->HasEntitlement(*Def, [this](const FHasEntitlementResult& R)
	{
		OnEntitlementChecked.Broadcast(R);
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
		OnStatIngested.Broadcast(R);
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

void UGamingServicesSubsystem::CreateSession(const FSessionSettings& Settings)
{
	Service->CreateSession(Settings, [this](const FSessionCreateResult& R)
	{
		OnSessionCreated.Broadcast(R);
	});
}

void UGamingServicesSubsystem::FindSessions(const FSessionSearchFilter& Filter)
{
	Service->FindSessions(Filter, [this](const FSessionSearchResult& R)
	{
		OnSessionsFound.Broadcast(R);
	});
}

void UGamingServicesSubsystem::JoinSession(const FSessionJoinHandle& JoinHandle)
{
	Service->JoinSession(JoinHandle, [this](const FSessionJoinResult& R)
	{
		OnSessionJoined.Broadcast(R);
	});
}

void UGamingServicesSubsystem::LeaveSession()
{
	Service->LeaveSession([this](const FGamingServiceResult& R)
	{
		OnSessionLeft.Broadcast(R);
	});
}

void UGamingServicesSubsystem::DestroySession()
{
	Service->DestroySession([this](const FGamingServiceResult& R)
	{
		OnSessionDestroyed.Broadcast(R);
	});
}

void UGamingServicesSubsystem::UpdateSession(const FSessionSettings& Settings)
{
	Service->UpdateSession(Settings, [this](const FGamingServiceResult& R)
	{
		OnSessionUpdated.Broadcast(R);
	});
}

void UGamingServicesSubsystem::LockLobby()
{
	Service->LockLobby([this](const FGamingServiceResult& R)
	{
		OnLobbyLocked.Broadcast(R);
	});
}

void UGamingServicesSubsystem::UnlockLobby()
{
	Service->UnlockLobby([this](const FGamingServiceResult& R)
	{
		OnLobbyUnlocked.Broadcast(R);
	});
}

void UGamingServicesSubsystem::GetCurrentSessionInfo()
{
	Service->GetCurrentSession([this](const FSessionInfo& Info)
	{
		UE_LOG(LogTemp, Log, TEXT("Current Session: %s"), *Info.SessionName);
	});
}

void UGamingServicesSubsystem::ShowInviteFriendsDialog()
{
	Service->ShowInviteFriendsDialog([this](const FGamingServiceResult& R)
	{
		OnInviteFriendsDialogShown.Broadcast(R);
	});
}

FString UGamingServicesSubsystem::GetSessionConnectionString() const
{
	return Service->GetSessionConnectionString();
}
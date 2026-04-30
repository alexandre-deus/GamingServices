#ifdef USE_STEAMWORKS

#include "Services/SteamworksGamingService.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PixelFormat.h"
#include "UObject/StrongObjectPtr.h"

#include "steam/steam_api.h"

struct FSteamworksSessionJoinHandle : public ISessionJoinHandle
{
	CSteamID LobbyId;
	
	FSteamworksSessionJoinHandle(const CSteamID& InLobbyId)
		: LobbyId(InLobbyId) {}
};

class FSteamworksGamingService::FSteamworksGamingServiceImpl
{
public:
	explicit FSteamworksGamingServiceImpl(FSteamworksGamingService* InOwner) :
		Owner(InOwner),
		m_CallbackLobbyChatUpdate(this, &FSteamworksGamingServiceImpl::OnLobbyChatUpdate),
		m_CallbackGameLobbyJoinRequested(this, &FSteamworksGamingServiceImpl::OnGameLobbyJoinRequested),
		m_CallbackAvatarImageLoaded(this, &FSteamworksGamingServiceImpl::OnAvatarImageLoaded)
	{
	}

	~FSteamworksGamingServiceImpl() { ShutdownSteamworks(); }

	bool Connect(const FSteamworksInitOptions& /*Opts*/)
	{
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Starting initialization..."));

		InitializeSteamworks();

		if (bIsInitialized)
		{
			UE_LOG(LogTemp, Log,
				   TEXT("SteamworksGamingService: Initialization completed "
						"successfully"));
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Initialization failed"));
			return false;
		}
	}

	void Shutdown()
	{
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Starting shutdown..."));

		bIsLoggedIn = false;
		UserId.Empty();
		DisplayName.Empty();
		AvatarCache.Empty();
		RequestedAvatars.Empty();

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Shutdown completed"));
	}

	void UnlockAchievement(const FString& AchievementId, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamUserStats,
			   TEXT("SteamworksGamingService: UnlockAchievement called when "
					"service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Unlocking achievement: %s"), *AchievementId);

		FTCHARToUTF8 UTF8String(*AchievementId);
		const char* AchievementIdUTF8 = UTF8String.Get();

		bool bAchievementExists = SteamUserStats->GetAchievement(AchievementIdUTF8, nullptr);
		if (!bAchievementExists)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Achievement does not exist: %s"), *AchievementId);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		bool bAlreadyUnlocked = false;
		SteamUserStats->GetAchievement(AchievementIdUTF8, &bAlreadyUnlocked);
		if (bAlreadyUnlocked)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Achievement already unlocked: %s"), *AchievementId);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		bool bSuccess = SteamUserStats->SetAchievement(AchievementIdUTF8);

		if (bSuccess)
		{
			SteamUserStats->StoreStats();
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Achievement unlocked successfully: %s"),
				   *AchievementId);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to unlock achievement: %s"), *AchievementId);
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamUserStats,
			   TEXT("SteamworksGamingService: QueryAchievements called when "
					"service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Querying achievements..."));

		TArray<FGameAchievement> Achievements;

		uint32 AchievementCount = SteamUserStats->GetNumAchievements();

		for (uint32 i = 0; i < AchievementCount; ++i)
		{
			FGameAchievement GameAchievement;

			const char* AchievementId = SteamUserStats->GetAchievementName(i);
			if (AchievementId)
			{
				GameAchievement.Id = UTF8_TO_TCHAR(AchievementId);

				const char* AchievementDisplayName =
					SteamUserStats->GetAchievementDisplayAttribute(AchievementId, "name");
				GameAchievement.DisplayName =
					AchievementDisplayName ? UTF8_TO_TCHAR(AchievementDisplayName) : GameAchievement.Id;

				const char* Description = SteamUserStats->GetAchievementDisplayAttribute(AchievementId, "desc");
				GameAchievement.Description = Description ? UTF8_TO_TCHAR(Description) : TEXT("");

				bool bUnlocked = false;
				SteamUserStats->GetAchievement(AchievementId, &bUnlocked);
				GameAchievement.bIsUnlocked = bUnlocked;

				float Progress = 0.0f;
				SteamUserStats->GetAchievementAchievedPercent(AchievementId, &Progress);
				GameAchievement.Progress = Progress;

				Achievements.Add(GameAchievement);
			}
		}

		if (Callback)
		{
			Callback(FAchievementsQueryResult(true, Achievements));
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Queried %d achievements"), Achievements.Num());
	}

	void ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamApps,
			   TEXT("SteamworksGamingService: ListEntitlements called when service not ready"));

		FFilesListResult Dummy;
		FEntitlementsListResult Result;
		Result.bSuccess = true;

		int32 DLCCount = SteamApps->GetDLCCount();
		for (int32 Index = 0; Index < DLCCount; ++Index)
		{
			AppId_t AppId = 0;
			bool bAvailable = false;
			char Name[256] = {0};

			if (SteamApps->BGetDLCDataByIndex(Index, &AppId, &bAvailable, Name, sizeof(Name)))
			{
				FEntitlement E;
				E.Id = FString::Printf(TEXT("%u"), (uint32)AppId);
				E.DisplayName = UTF8_TO_TCHAR(Name);
				E.Description = TEXT("");
				Result.Entitlements.Add(E);
			}
		}

		if (Callback)
		{
			Callback(Result);
		}
	}

	void HasEntitlement(const FEntitlementDefinition& Definition,
	                    TFunction<void(const FHasEntitlementResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamApps,
			   TEXT("SteamworksGamingService: HasEntitlement called when service not ready"));

		AppId_t AppId = (AppId_t)Definition.SteamAppId;
		FHasEntitlementResult Result;
		Result.EntitlementId = FString::Printf(TEXT("%u"), (uint32)AppId);

		if (AppId == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: HasEntitlement called with AppId 0"));
			Result.bSuccess = false;
			Result.bHasEntitlement = false;
		}
		else
		{
			bool bOwned = SteamApps->BIsSubscribedApp(AppId) || SteamApps->BIsDlcInstalled(AppId);
			Result.bSuccess = true;
			Result.bHasEntitlement = bOwned;
		}

		if (Callback)
		{
			Callback(Result);
		}
	}

	void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
							   TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamUserStats,
			   TEXT("SteamworksGamingService: WriteLeaderboardScore called when "
					"service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Writing leaderboard score: %d to %s"), Score,
			   *LeaderboardId);

		FTCHARToUTF8 UTF8String(*LeaderboardId);
		const char* LeaderboardIdUTF8 = UTF8String.Get();

		SteamAPICall_t CallHandle = SteamUserStats->FindOrCreateLeaderboard(
			LeaderboardIdUTF8, k_ELeaderboardSortMethodDescending, k_ELeaderboardDisplayTypeNumeric);
		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: FindOrCreateLeaderboard issued, "
					"CallHandle=%llu"),
			   CallHandle);

		if (CallHandle == k_uAPICallInvalid)
		{
			UE_LOG(LogTemp, Error,
				   TEXT("SteamworksGamingService: Failed to create API call for "
						"leaderboard: %s"),
				   *LeaderboardId);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		CallResults.Add<LeaderboardFindResult_t>(
			CallHandle,
			[this, Score, Callback](const LeaderboardFindResult_t& Find, bool bIOFailure)
			{
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: CallResult - WriteLeaderboardScore "
							"FindLeaderboard callback (Found=%d, IOFail=%d, Handle=%llu)"),
					   (int32)Find.m_bLeaderboardFound, (int32)bIOFailure, (uint64)Find.m_hSteamLeaderboard);
				if (bIOFailure || !Find.m_bLeaderboardFound)
				{
					if (Callback)
					{
						Callback(FGamingServiceResult(false));
					}
					return;
				}
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: Proceeding to upload score to "
							"Leaderboard=%llu"),
					   (uint64)Find.m_hSteamLeaderboard);
				HandleUploadLeaderboardScore(Find.m_hSteamLeaderboard, Score, Callback);
			});
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaderboard write initiated for: %s"), *LeaderboardId);
	}

	void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
							  TFunction<void(const FLeaderboardResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamUserStats,
			   TEXT("SteamworksGamingService: QueryLeaderboardPage called when "
					"service not ready"));

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: Querying leaderboard page: %s "
					"(Start: %d, Limit: %d)"),
			   *LeaderboardId, ContinuationToken, Limit);

		FTCHARToUTF8 UTF8String(*LeaderboardId);
		const char* LeaderboardIdUTF8 = UTF8String.Get();

		SteamAPICall_t CallHandle = SteamUserStats->FindLeaderboard(LeaderboardIdUTF8);
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: FindLeaderboard issued, CallHandle=%llu"), CallHandle);

		if (CallHandle == k_uAPICallInvalid)
		{
			UE_LOG(LogTemp, Error,
				   TEXT("SteamworksGamingService: Failed to create API call for "
						"leaderboard query: %s"),
				   *LeaderboardId);
			if (Callback)
			{
				Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
			}
			return;
		}

		CallResults.Add<LeaderboardFindResult_t>(
			CallHandle,
			[this, LeaderboardId, Limit, ContinuationToken, Callback](const LeaderboardFindResult_t& Find,
																	  bool bIOFailure)
			{
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: CallResult - QueryLeaderboardPage "
							"FindLeaderboard callback (Found=%d, IOFail=%d, Handle=%llu)"),
					   (int32)Find.m_bLeaderboardFound, (int32)bIOFailure, (uint64)Find.m_hSteamLeaderboard);
				if (bIOFailure || !Find.m_bLeaderboardFound)
				{
					if (Callback)
					{
						Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
					}
					return;
				}
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: Proceeding to download entries for "
							"Leaderboard=%llu, Start=%d, Limit=%d"),
					   (uint64)Find.m_hSteamLeaderboard, ContinuationToken, Limit);
				HandleDownloadLeaderboardEntries(Find.m_hSteamLeaderboard, LeaderboardId, Limit, ContinuationToken,
												 Callback);
			});
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaderboard query initiated for: %s"), *LeaderboardId);
	}

	void IngestStat(const FString& StatName, int32 Amount, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamUserStats,
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

	void QueryStat(const FString& StatName, TFunction<void(const FStatQueryResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamUserStats,
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

	void WriteFile(const FString& FilePath, const TArray<uint8>& Data,
				   TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: WriteFile called when service not "
					"ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Writing file to cloud: %s (%d bytes)"), *FilePath,
			   Data.Num());

		FTCHARToUTF8 UTF8String(*FilePath);
		const char* FilePathUTF8 = UTF8String.Get();

		bool bSuccess = SteamRemoteStorage->FileWrite(FilePathUTF8, Data.GetData(), Data.Num());

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log,
				   TEXT("SteamworksGamingService: File written to cloud "
						"successfully: %s"),
				   *FilePath);
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
				   TEXT("SteamworksGamingService: Failed to write file to cloud "
						"storage: %s"),
				   *FilePath);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
		}
	}

	void ReadFile(const FString& FilePath, TFunction<void(const FFileReadResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: ReadFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Reading file from cloud: %s"), *FilePath);

		FTCHARToUTF8 UTF8String(*FilePath);
		const char* FilePathUTF8 = UTF8String.Get();

		bool bFileExists = SteamRemoteStorage->FileExists(FilePathUTF8);
		if (!bFileExists)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("SteamworksGamingService: File does not exist in cloud "
						"storage: %s"),
				   *FilePath);
			if (Callback)
			{
				Callback(FFileReadResult(false, FilePath));
			}
			return;
		}

		int32 FileSize = SteamRemoteStorage->GetFileSize(FilePathUTF8);
		if (FileSize <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Invalid file size for: %s"), *FilePath);
			if (Callback)
			{
				Callback(FFileReadResult(false, FilePath));
			}
			return;
		}

		TArray<uint8> FileData;
		FileData.SetNum(FileSize);
		int32 BytesRead = SteamRemoteStorage->FileRead(FilePathUTF8, FileData.GetData(), FileSize);

		if (BytesRead != FileSize)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to read file from cloud: %s"), *FilePath);
			if (Callback)
			{
				Callback(FFileReadResult(false, FilePath));
			}
			return;
		}

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: File read from cloud successfully: "
					"%s (%d bytes)"),
			   *FilePath, FileSize);
		if (Callback)
		{
			Callback(FFileReadResult(true, FilePath, FileData));
		}
	}

	void DeleteFile(const FString& FilePath, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: DeleteFile called when service not "
					"ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Deleting file: %s"), *FilePath);

		FTCHARToUTF8 UTF8String(*FilePath);
		const char* FilePathUTF8 = UTF8String.Get();

		bool bSuccess = SteamRemoteStorage->FileDelete(FilePathUTF8);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: File deleted successfully: %s"), *FilePath);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("SteamworksGamingService: File deletion failed or file did "
						"not exist: %s"),
				   *FilePath);
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void ListFiles(const FString& DirectoryPath, TFunction<void(const FFilesListResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: ListFiles called when service not "
					"ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Listing files in directory: %s"), *DirectoryPath);

		FFilesListResult Result;
		Result.bSuccess = true;

		int32 FileCount = SteamRemoteStorage->GetFileCount();

		for (int32 i = 0; i < FileCount; ++i)
		{
			int32 FileSize = 0;
			const char* FileName = SteamRemoteStorage->GetFileNameAndSize(i, &FileSize);

			if (FileName && FileSize > 0)
			{
				FString FileNameStr = UTF8_TO_TCHAR(FileName);

				if (DirectoryPath.IsEmpty() || FileNameStr.StartsWith(DirectoryPath))
				{
					FFileBlobData FileData;
					FileData.FilePath = FileNameStr;
					FileData.Size = FileSize;

					int64 Timestamp = SteamRemoteStorage->GetFileTimestamp(FileName);
					if (Timestamp > 0)
					{
						FileData.LastModified = FDateTime::FromUnixTimestamp(Timestamp);
					}
					else
					{
						FileData.LastModified = FDateTime::Now();
					}

					Result.Files.Add(FileData);
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Found %d files"), Result.Files.Num());
		if (Callback)
		{
			Callback(Result);
		}
	}

	void Tick()
	{
		if (bIsInitialized)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("SteamworksGamingService: Tick - Running SteamAPI callbacks"));
			SteamAPI_RunCallbacks();
			UE_LOG(LogTemp, VeryVerbose, TEXT("SteamworksGamingService: Tick - Pumping call results"));
			CallResults.Pump();
			
			// Process pending lobby search contexts
			ProcessLobbySearchContexts();
		}
	}

	void ProcessLobbySearchContexts()
	{
		if (ActiveSearchContexts.Num() == 0)
		{
			return;
		}

		double CurrentTime = FPlatformTime::Seconds();
		TArray<TSharedPtr<FLobbySearchContext>> CompletedContexts;

		for (TSharedPtr<FLobbySearchContext>& Context : ActiveSearchContexts)
		{
			if (!Context.IsValid())
			{
				CompletedContexts.Add(Context);
				continue;
			}

			// Check for timeout
			bool bTimedOut = (CurrentTime - Context->StartTime) > Context->TimeoutSeconds;
			if (bTimedOut && Context->PendingLobbyIDs.Num() > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Lobby search timed out, %d lobbies still pending"), Context->PendingLobbyIDs.Num());
				// Continue anyway with whatever data we have
				Context->PendingLobbyIDs.Empty();
			}

			// Check each pending lobby to see if data is now available
			TArray<uint64> CompletedLobbies;
			for (uint64 LobbyID : Context->PendingLobbyIDs)
			{
				CSteamID SteamID(LobbyID);
				int32 DataCount = SteamMatchmaking->GetLobbyDataCount(SteamID);
				
				// If we have data now, mark this lobby as complete
				if (DataCount > 0)
				{
					CompletedLobbies.Add(LobbyID);
					UE_LOG(LogTemp, Verbose, TEXT("SteamworksGamingService: Lobby %llu data ready (%d entries)"), LobbyID, DataCount);
				}
			}

			// Remove completed lobbies from pending set
			for (uint64 CompletedID : CompletedLobbies)
			{
				Context->PendingLobbyIDs.Remove(CompletedID);
			}

			// If all lobbies have data (or timed out), finalize this search
			if (Context->PendingLobbyIDs.Num() == 0)
			{
				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: All lobby data received, finalizing search"));
				FinalizeLobbySearch(Context);
				CompletedContexts.Add(Context);
			}
		}

		// Remove completed contexts
		for (const TSharedPtr<FLobbySearchContext>& Completed : CompletedContexts)
		{
			ActiveSearchContexts.Remove(Completed);
		}
	}

	bool IsInitialized() const { return bIsInitialized; }
	bool IsConnected() const { return bIsInitialized; }
	bool IsLoggedIn() const { return bIsLoggedIn; }
	bool NeedsLogin() const { return false; }
	const FString& GetUserId() const { return UserId; }
	const FString& GetDisplayName() const { return DisplayName; }

	UTexture2D* GetAvatar()
	{
		if (!bIsLoggedIn || !SteamUser)
		{
			return nullptr;
		}
		return GetAvatarForSteamID(SteamUser->GetSteamID());
	}

	UTexture2D* GetAvatarByUserId(const FString& InUserId)
	{
		const uint64 SteamID64 = FCString::Strtoui64(*InUserId, nullptr, 10);
		if (SteamID64 == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: GetAvatarByUserId called with invalid UserId '%s'"), *InUserId);
			return nullptr;
		}
		return GetAvatarForSteamID(CSteamID(SteamID64));
	}

	bool IsSteamRunning() const { return SteamAPI_IsSteamRunning(); }

	bool IsSteamOverlayEnabled() const { return SteamUtils ? SteamUtils->IsOverlayEnabled() : false; }

	FString GetSessionConnectionString() const
	{
		if (!bIsInLobby || !CurrentLobbyId.IsValid() || !SteamMatchmaking)
		{
			return FString();
		}

		CSteamID OwnerID = SteamMatchmaking->GetLobbyOwner(CurrentLobbyId);
		return FString::Printf(TEXT("steam.%llu"), OwnerID.ConvertToUint64());
	}

	void CreateSession(const FSessionSettings& Settings, TFunction<void(const FSessionCreateResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: CreateSession called when service not ready"));

		if (bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Already in a lobby, leaving old lobby first"));
			DestroySession([this, Settings, Callback](const FGamingServiceResult& Result)
			{
				CreateSession(Settings, Callback);
			});
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Creating lobby: %s"), *Settings.SessionName);

		ELobbyType LobbyType = k_ELobbyTypePublic;
		if (Settings.Privacy == ESessionPrivacy::Private)
			LobbyType = k_ELobbyTypePrivate;
		else if (Settings.Privacy == ESessionPrivacy::FriendsOnly)
			LobbyType = k_ELobbyTypeFriendsOnly;

		SteamAPICall_t Handle = SteamMatchmaking->CreateLobby(LobbyType, Settings.MaxPlayers);
		
		CallResults.Add<LobbyCreated_t>(Handle, [this, Settings, Callback](const LobbyCreated_t& Result, bool bIOFailure)
		{
			FSessionCreateResult CreateResult;
			CreateResult.bSuccess = !bIOFailure && Result.m_eResult == k_EResultOK;

			if (CreateResult.bSuccess)
			{
				CurrentLobbyId = CSteamID(Result.m_ulSteamIDLobby);
				bIsInLobby = true;
				bIsLobbyHost = true;

				// Snapshot current lobby members
				CurrentLobbyMembers.Empty();
				int32 MemberCount = SteamMatchmaking->GetNumLobbyMembers(CurrentLobbyId);
				for (int32 i = 0; i < MemberCount; ++i)
				{
					CSteamID Member = SteamMatchmaking->GetLobbyMemberByIndex(CurrentLobbyId, i);
					CurrentLobbyMembers.Add(Member.ConvertToUint64());
				}

				SteamMatchmaking->SetLobbyData(CurrentLobbyId, "name", TCHAR_TO_UTF8(*Settings.SessionName));

				for (const FSessionAttribute& Attr : Settings.CustomAttributes)
				{
					SteamMatchmaking->SetLobbyData(CurrentLobbyId, TCHAR_TO_UTF8(*Attr.Key), TCHAR_TO_UTF8(*Attr.Value));
				}

				CreateResult.SessionInfo.SessionName = Settings.SessionName;
				CreateResult.SessionInfo.HostUserId = UserId;
				CreateResult.SessionInfo.HostDisplayName = DisplayName;
				CreateResult.SessionInfo.MaxPlayers = Settings.MaxPlayers;
				CreateResult.SessionInfo.CurrentPlayers = 1;
				CreateResult.SessionInfo.AvailableSlots = Settings.MaxPlayers - 1;

				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby created successfully: %llu"), CurrentLobbyId.ConvertToUint64());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to create lobby"));
			}

			if (Callback)
			{
				Callback(CreateResult);
			}
		});
	}

	void FindSessions(const FSessionSearchFilter& Filter, TFunction<void(const FSessionSearchResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: FindSessions called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Searching for lobbies, max results: %d"), Filter.MaxResults);

		SteamMatchmaking->AddRequestLobbyListResultCountFilter(Filter.MaxResults);

		for (const FSessionAttribute& Attr : Filter.RequiredAttributes)
		{
			SteamMatchmaking->AddRequestLobbyListStringFilter(TCHAR_TO_UTF8(*Attr.Key), TCHAR_TO_UTF8(*Attr.Value), k_ELobbyComparisonEqual);
		}

		SteamAPICall_t Handle = SteamMatchmaking->RequestLobbyList();

		TSharedPtr<FLobbySearchContext> SearchContext = MakeShared<FLobbySearchContext>();
		SearchContext->Impl = this;
		SearchContext->Callback = MoveTemp(Callback);
		SearchContext->StartTime = FPlatformTime::Seconds();

		ISteamMatchmaking* MatchmakingPtr = SteamMatchmaking;

		CallResults.Add<LobbyMatchList_t>(Handle, [SearchContext, MatchmakingPtr](const LobbyMatchList_t& Result, bool bIOFailure)
		{
			if (bIOFailure)
			{
				UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Lobby search failed"));
				FSessionSearchResult ErrorResult;
				ErrorResult.bSuccess = false;
				if (SearchContext->Callback)
				{
					SearchContext->Callback(ErrorResult);
				}
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Found %d lobbies, requesting metadata..."), Result.m_nLobbiesMatching);

			if (Result.m_nLobbiesMatching == 0)
			{
				FSessionSearchResult EmptyResult;
				EmptyResult.bSuccess = true;
				if (SearchContext->Callback)
				{
					SearchContext->Callback(EmptyResult);
				}
				return;
			}

			// Collect lobby IDs
			for (uint32 i = 0; i < Result.m_nLobbiesMatching; ++i)
			{
				CSteamID LobbyId = MatchmakingPtr->GetLobbyByIndex(i);
				SearchContext->LobbyIDs.Add(LobbyId);
				SearchContext->PendingLobbyIDs.Add(LobbyId.ConvertToUint64());
			}

			// Request metadata for all lobbies (async, triggers LobbyDataUpdate_t callbacks)
			for (const CSteamID& LobbyId : SearchContext->LobbyIDs)
			{
				MatchmakingPtr->RequestLobbyData(LobbyId);
			}

			// Add to active contexts - will be processed in Tick() when data arrives
			SearchContext->Impl->ActiveSearchContexts.Add(SearchContext);
			
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Waiting for %d lobby metadata updates..."), SearchContext->PendingLobbyIDs.Num());
		});
	}

	struct FLobbySearchContext
	{
		FSteamworksGamingServiceImpl* Impl;
		TFunction<void(const FSessionSearchResult&)> Callback;
		TSet<uint64> PendingLobbyIDs;  // Lobbies waiting for data
		TArray<CSteamID> LobbyIDs;      // All lobbies
		double StartTime = 0.0;         // When the search started
		double TimeoutSeconds = 5.0;    // Max time to wait for lobby data
	};

	void FinalizeLobbySearch(TSharedPtr<FLobbySearchContext> SearchContext)
	{
		FSessionSearchResult FinalResult;
		FinalResult.bSuccess = true;

		for (const CSteamID& LobbyId : SearchContext->LobbyIDs)
		{
			FSessionInfo Session;
			Session.JoinHandle.BackendHandle = MakeShared<FSteamworksSessionJoinHandle>(LobbyId);

			const char* LobbyName = SteamMatchmaking->GetLobbyData(LobbyId, "name");
			Session.SessionName = LobbyName;

			CSteamID OwnerID = SteamMatchmaking->GetLobbyOwner(LobbyId);
			Session.HostUserId = FString::Printf(TEXT("%llu"), OwnerID.ConvertToUint64());
			Session.HostDisplayName = UTF8_TO_TCHAR(SteamFriends->GetFriendPersonaName(OwnerID));

			Session.MaxPlayers = SteamMatchmaking->GetLobbyMemberLimit(LobbyId);
			Session.CurrentPlayers = SteamMatchmaking->GetNumLobbyMembers(LobbyId);
			Session.AvailableSlots = Session.MaxPlayers - Session.CurrentPlayers;

			int32 DataCount = SteamMatchmaking->GetLobbyDataCount(LobbyId);
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby %llu | Name='%s' | Owner=%llu | Players=%d/%d | Metadata (%d entries):"),
				LobbyId.ConvertToUint64(),
				*Session.SessionName,
				OwnerID.ConvertToUint64(),
				Session.CurrentPlayers,
				Session.MaxPlayers,
				DataCount);
			
			for (int32 j = 0; j < DataCount; ++j)
			{
				char Key[256] = {0};
				char Value[256] = {0};
				if (SteamMatchmaking->GetLobbyDataByIndex(LobbyId, j, Key, 256, Value, 256))
				{
					UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService:   [%d] '%s' = '%s'"), j, UTF8_TO_TCHAR(Key), UTF8_TO_TCHAR(Value));
					FSessionAttribute Attr;
					Attr.Key = UTF8_TO_TCHAR(Key);
					Attr.Value = UTF8_TO_TCHAR(Value);
					Session.CustomAttributes.Add(Attr);
				}
			}

			FinalResult.Sessions.Add(Session);
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Returning %d lobbies with metadata"), FinalResult.Sessions.Num());

		if (SearchContext->Callback)
		{
			SearchContext->Callback(FinalResult);
		}
	}

	void JoinSession(const FSessionJoinHandle& JoinHandle, TFunction<void(const FSessionJoinResult&)> Callback) {
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: JoinSession called when service not ready"));

		
		TSharedPtr<FSteamworksSessionJoinHandle> SteamworksHandle = StaticCastSharedPtr<FSteamworksSessionJoinHandle>(JoinHandle.BackendHandle);
		if (!SteamworksHandle->LobbyId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: JoinSession called with an invalid LobbyId."));
			FSessionJoinResult FailResult;
			FailResult.bSuccess = false;
			Callback(FailResult);
			return;
		}

		if (bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Already in a lobby, leaving old lobby first"));
			LeaveSession([this, JoinHandle, Callback](const FGamingServiceResult& Result)
			{
				JoinSession(JoinHandle, Callback);
			});
			return;
		}

		uint64 SteamIdUint64 = SteamworksHandle->LobbyId.ConvertToUint64();
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Joining lobby: %llu"), SteamIdUint64);

		SteamAPICall_t Handle = SteamMatchmaking->JoinLobby(SteamworksHandle->LobbyId);

		CallResults.Add<LobbyEnter_t>(Handle, [this, Callback](const LobbyEnter_t& Result, bool bIOFailure)
		{
			FSessionJoinResult JoinResult;
			JoinResult.bSuccess = !bIOFailure && (Result.m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess);

			if (JoinResult.bSuccess)
			{
				CurrentLobbyId = CSteamID(Result.m_ulSteamIDLobby);
				CurrentHostId = SteamMatchmaking->GetLobbyOwner(CurrentLobbyId);
				bIsInLobby = true;
				bIsLobbyHost = false;

				// Snapshot current lobby members
				CurrentLobbyMembers.Empty();
				int32 MemberCount = SteamMatchmaking->GetNumLobbyMembers(CurrentLobbyId);
				for (int32 i = 0; i < MemberCount; ++i)
				{
					CSteamID Member = SteamMatchmaking->GetLobbyMemberByIndex(CurrentLobbyId, i);
					CurrentLobbyMembers.Add(Member.ConvertToUint64());
				}
				
				const char* LobbyName = SteamMatchmaking->GetLobbyData(CurrentLobbyId, "name");
				JoinResult.SessionInfo.SessionName = UTF8_TO_TCHAR(LobbyName);

				CSteamID OwnerID = SteamMatchmaking->GetLobbyOwner(CurrentLobbyId);
				JoinResult.SessionInfo.HostUserId = FString::Printf(TEXT("%llu"), OwnerID.ConvertToUint64());
				JoinResult.SessionInfo.HostDisplayName = UTF8_TO_TCHAR(SteamFriends->GetFriendPersonaName(OwnerID));

				JoinResult.SessionInfo.MaxPlayers = SteamMatchmaking->GetLobbyMemberLimit(CurrentLobbyId);
				JoinResult.SessionInfo.CurrentPlayers = SteamMatchmaking->GetNumLobbyMembers(CurrentLobbyId);
				JoinResult.SessionInfo.AvailableSlots = JoinResult.SessionInfo.MaxPlayers - JoinResult.SessionInfo.CurrentPlayers;

				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Successfully joined lobby"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to join lobby"));
			}

			if (Callback)
			{
				Callback(JoinResult);
			}
		});
	}

	void LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: LeaveSession called when service not ready"));

		if (!bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Not in a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaving lobby"));

		SteamMatchmaking->LeaveLobby(CurrentLobbyId);
		
		bIsInLobby = false;
		bIsLobbyHost = false;
		CurrentLobbyId = CSteamID();
		CurrentLobbyMembers.Empty();

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Successfully left lobby"));

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

	void DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: DestroySession called when service not ready"));

		if (!bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Not in a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Destroying lobby"));

		SteamMatchmaking->LeaveLobby(CurrentLobbyId);
		
		bIsInLobby = false;
		bIsLobbyHost = false;
		CurrentLobbyId = CSteamID();
		CurrentLobbyMembers.Empty();

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Successfully destroyed lobby"));

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

	void UpdateSession(const FSessionSettings& Settings, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: UpdateSession called when service not ready"));

		if (!bIsInLobby || !bIsLobbyHost)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot update lobby - not hosting a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Updating lobby"));

		bool bSuccess = true;

		if (!Settings.SessionName.IsEmpty())
		{
			bSuccess &= SteamMatchmaking->SetLobbyData(CurrentLobbyId, "name", TCHAR_TO_UTF8(*Settings.SessionName));
		}
		
		if (Settings.MaxPlayers > 0)
		{
			bSuccess &= SteamMatchmaking->SetLobbyMemberLimit(CurrentLobbyId, Settings.MaxPlayers);
		}

		for (const FSessionAttribute& Attr : Settings.CustomAttributes)
		{
			bSuccess &= SteamMatchmaking->SetLobbyData(CurrentLobbyId, TCHAR_TO_UTF8(*Attr.Key), TCHAR_TO_UTF8(*Attr.Value));
		}

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby updated successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to update some lobby data"));
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void LockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: LockLobby called when service not ready"));

		if (!bIsInLobby || !bIsLobbyHost)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot lock lobby - not hosting a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		const bool bSuccess = SteamMatchmaking->SetLobbyJoinable(CurrentLobbyId, false);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby locked successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to lock lobby"));
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamMatchmaking,
		       TEXT("SteamworksGamingService: UnlockLobby called when service not ready"));

		if (!bIsInLobby || !bIsLobbyHost)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot unlock lobby - not hosting a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		const bool bSuccess = SteamMatchmaking->SetLobbyJoinable(CurrentLobbyId, true);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby unlocked successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to unlock lobby"));
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
	{
		FSessionInfo Info;
		
		if (bIsInLobby && CurrentLobbyId.IsValid())
		{
			const char* LobbyName = SteamMatchmaking->GetLobbyData(CurrentLobbyId, "name");
			Info.SessionName = LobbyName;

			CSteamID OwnerID = SteamMatchmaking->GetLobbyOwner(CurrentLobbyId);
			Info.HostUserId = FString::Printf(TEXT("%llu"), OwnerID.ConvertToUint64());
			Info.HostDisplayName = UTF8_TO_TCHAR(SteamFriends->GetFriendPersonaName(OwnerID));

			Info.MaxPlayers = SteamMatchmaking->GetLobbyMemberLimit(CurrentLobbyId);
			Info.CurrentPlayers = SteamMatchmaking->GetNumLobbyMembers(CurrentLobbyId);
			Info.AvailableSlots = Info.MaxPlayers - Info.CurrentPlayers;
		}

		if (Callback)
		{
			Callback(Info);
		}
	}

	void ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SteamFriends,
		       TEXT("SteamworksGamingService: ShowInviteFriendsDialog called when service not ready"));

		if (!bIsInLobby || !CurrentLobbyId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot show invite dialog - not in a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Opening Steam invite overlay for lobby %llu"), CurrentLobbyId.ConvertToUint64());

		char LobbyIdStr[64];
		FCStringAnsi::Snprintf(LobbyIdStr, sizeof(LobbyIdStr), "%llu", CurrentLobbyId.ConvertToUint64());
		SteamFriends->ActivateGameOverlayInviteDialog(CurrentLobbyId);

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

private:
	friend class FSteamworksGamingService;

	FSteamworksGamingService* Owner;

	bool bIsInitialized = false;
	bool bIsLoggedIn = false;
	FString UserId;
	FString DisplayName;

	ISteamUserStats* SteamUserStats = nullptr;
	ISteamUser* SteamUser = nullptr;
	ISteamUtils* SteamUtils = nullptr;
	ISteamFriends* SteamFriends = nullptr;
	ISteamRemoteStorage* SteamRemoteStorage = nullptr;
	ISteamMatchmaking* SteamMatchmaking = nullptr;
	ISteamApps* SteamApps = nullptr;

	CSteamID CurrentLobbyId;
	CSteamID CurrentHostId;
	bool bIsInLobby = false;
	bool bIsLobbyHost = false;
	TSet<uint64> CurrentLobbyMembers;

	TArray<TSharedPtr<FLobbySearchContext>> ActiveSearchContexts;

	// Manual CCallback members for lobby events
	CCallback<FSteamworksGamingServiceImpl, LobbyChatUpdate_t> m_CallbackLobbyChatUpdate;
	CCallback<FSteamworksGamingServiceImpl, GameLobbyJoinRequested_t> m_CallbackGameLobbyJoinRequested;
	CCallback<FSteamworksGamingServiceImpl, AvatarImageLoaded_t> m_CallbackAvatarImageLoaded;

	TMap<uint64, TStrongObjectPtr<UTexture2D>> AvatarCache;
	TSet<uint64> RequestedAvatars;

	FCriticalSection CallbackCriticalSection;

	void OnLobbyChatUpdate(LobbyChatUpdate_t* pParam)
	{
		if (!bIsInLobby || pParam->m_ulSteamIDLobby != CurrentLobbyId.ConvertToUint64())
		{
			return;
		}

		CSteamID ChangedUser(pParam->m_ulSteamIDUserChanged);
		FString ChangedUserId = FString::Printf(TEXT("%llu"), ChangedUser.ConvertToUint64());
		FString ChangedDisplayName = SteamFriends ? UTF8_TO_TCHAR(SteamFriends->GetFriendPersonaName(ChangedUser)) : TEXT("Unknown");

		if (pParam->m_rgfChatMemberStateChange & k_EChatMemberStateChangeEntered)
		{
			CurrentLobbyMembers.Add(ChangedUser.ConvertToUint64());
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: User joined lobby: %s (%s)"), *ChangedDisplayName, *ChangedUserId);
			if (Owner->OnSessionUserJoined)
			{
				Owner->OnSessionUserJoined(FSessionMemberInfo(ChangedUserId, ChangedDisplayName));
			}
		}

		if (pParam->m_rgfChatMemberStateChange & (k_EChatMemberStateChangeLeft | k_EChatMemberStateChangeDisconnected | k_EChatMemberStateChangeKicked | k_EChatMemberStateChangeBanned))
		{
			CurrentLobbyMembers.Remove(ChangedUser.ConvertToUint64());
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: User left lobby: %s (%s)"), *ChangedDisplayName, *ChangedUserId);
			if (Owner->OnSessionUserLeft)
			{
				Owner->OnSessionUserLeft(FSessionMemberInfo(ChangedUserId, ChangedDisplayName));
			}
			if (ChangedUser == CurrentHostId)
			{
				UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Host has left the lobby"));
				if (Owner->OnSessionEnded)
				{
					Owner->OnSessionEnded(FGamingServiceResult(true));
				}
			}
		}
	}

	void OnAvatarImageLoaded(AvatarImageLoaded_t* pParam)
	{
		const uint64 SteamID64 = pParam->m_steamID.ConvertToUint64();
		// Only react to users we've explicitly requested (or already cached); avoids building
		// textures for unrelated avatars Steam happens to load (e.g. friend list portraits).
		if (!RequestedAvatars.Contains(SteamID64) && !AvatarCache.Contains(SteamID64))
		{
			return;
		}
		BuildAvatarFromHandle(SteamID64, pParam->m_iImage);
	}

	UTexture2D* GetAvatarForSteamID(const CSteamID& SteamID)
	{
		if (!SteamFriends || !SteamUtils)
		{
			return nullptr;
		}

		const uint64 SteamID64 = SteamID.ConvertToUint64();
		if (TStrongObjectPtr<UTexture2D>* Existing = AvatarCache.Find(SteamID64))
		{
			if (Existing->IsValid())
			{
				return Existing->Get();
			}
		}

		// Make sure persona/avatar info is queued for download for non-friend users.
		// Returns false if the data is already available, true if a request was started.
		SteamFriends->RequestUserInformation(SteamID, /*bRequireNameOnly=*/false);

		const int AvatarHandle = SteamFriends->GetLargeFriendAvatar(SteamID);
		if (AvatarHandle == 0)
		{
			// Not loaded yet — Steam will fire AvatarImageLoaded_t when ready.
			RequestedAvatars.Add(SteamID64);
			return nullptr;
		}
		if (AvatarHandle < 0)
		{
			// User has no avatar set.
			return nullptr;
		}

		BuildAvatarFromHandle(SteamID64, AvatarHandle);
		if (TStrongObjectPtr<UTexture2D>* Built = AvatarCache.Find(SteamID64))
		{
			return Built->Get();
		}
		return nullptr;
	}

	void BuildAvatarFromHandle(uint64 SteamID64, int AvatarHandle)
	{
		if (AvatarHandle <= 0 || !SteamUtils)
		{
			return;
		}

		uint32 Width = 0;
		uint32 Height = 0;
		if (!SteamUtils->GetImageSize(AvatarHandle, &Width, &Height) || Width == 0 || Height == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Failed to get avatar image size for %llu"), SteamID64);
			return;
		}

		const int32 BufferSize = static_cast<int32>(Width * Height * 4);
		TArray<uint8> RGBA;
		RGBA.SetNumUninitialized(BufferSize);
		if (!SteamUtils->GetImageRGBA(AvatarHandle, RGBA.GetData(), BufferSize))
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Failed to read avatar pixel data for %llu"), SteamID64);
			return;
		}

		// Steam returns pixels as RGBA; UTexture2D B8G8R8A8 expects BGRA — swap channels.
		for (int32 i = 0; i + 3 < RGBA.Num(); i += 4)
		{
			Swap(RGBA[i + 0], RGBA[i + 2]);
		}

		UTexture2D* Texture = UTexture2D::CreateTransient(static_cast<int32>(Width), static_cast<int32>(Height), PF_B8G8R8A8);
		if (!Texture)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: CreateTransient avatar texture failed for %llu"), SteamID64);
			return;
		}
		Texture->SRGB = true;

		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(TextureData, RGBA.GetData(), RGBA.Num());
		Mip.BulkData.Unlock();
		Texture->UpdateResource();

		AvatarCache.Emplace(SteamID64, TStrongObjectPtr<UTexture2D>(Texture));
		RequestedAvatars.Remove(SteamID64);
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Avatar loaded for %llu (%ux%u)"), SteamID64, Width, Height);
	}

	void OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* pParam)
	{
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby invite accepted for lobby %llu"), pParam->m_steamIDLobby.ConvertToUint64());

		CSteamID FriendId = pParam->m_steamIDFriend;
		FString FriendUserId = FString::Printf(TEXT("%llu"), FriendId.ConvertToUint64());
		FString FriendDisplayName = SteamFriends ? UTF8_TO_TCHAR(SteamFriends->GetFriendPersonaName(FriendId)) : TEXT("Unknown");

		FLobbyInviteAcceptedInfo Info;
		Info.InviterUserId = FriendUserId;
		Info.InviterDisplayName = FriendDisplayName;
		Info.JoinHandle.BackendHandle = MakeShared<FSteamworksSessionJoinHandle>(pParam->m_steamIDLobby);

		if (Owner->OnLobbyInviteAccepted)
		{
			Owner->OnLobbyInviteAccepted(Info);
		}
	}

	struct FCallResultManager
	{
		ISteamUtils* SteamUtilsPtr = nullptr;
		TMap<SteamAPICall_t, TFunction<void(SteamAPICall_t, bool)>> Entries;
		TArray<TPair<SteamAPICall_t, TFunction<void(SteamAPICall_t, bool)>>> PendingEntries;
		bool bIsPumping = false;

		void Initialize(ISteamUtils* InSteamUtils) { SteamUtilsPtr = InSteamUtils; }

		template <typename T>
		void Add(SteamAPICall_t Handle, TFunction<void(const T&, bool)> OnComplete)
		{
			ISteamUtils* Utils = SteamUtilsPtr;
			UE_LOG(LogTemp, Log,
				   TEXT("SteamworksGamingService: CallResults.Add - Registering "
						"callback (Handle=%llu, CallbackId=%d)"),
				   (uint64)Handle, (int32)T::k_iCallback);
			auto Wrapped = [OnComplete, Utils](SteamAPICall_t H, bool bIOFailure)
			{
				T Result{};
				uint32 CubResult = sizeof(T);
				bool bGot = Utils->GetAPICallResult(H, &Result, CubResult, T::k_iCallback, nullptr);
				if (!bGot)
				{
					OnComplete(Result, true);
					return;
				}
				OnComplete(Result, bIOFailure);
			};
			if (bIsPumping)
			{
				UE_LOG(LogTemp, Verbose,
					   TEXT("SteamworksGamingService: CallResults.Add - Deferring add "
							"while pumping (Handle=%llu)"),
					   (uint64)Handle);
				PendingEntries.Add(
					TPair<SteamAPICall_t, TFunction<void(SteamAPICall_t, bool)>>(Handle, MoveTemp(Wrapped)));
			}
			else
			{
				Entries.Add(Handle, MoveTemp(Wrapped));
			}
		}

		void Pump()
		{
			TArray<SteamAPICall_t> CompletedCalls;
			
			for (auto It = Entries.CreateIterator(); It; ++It)
			{
				bool bFailed = false;
				bool bCompleted = SteamUtilsPtr->IsAPICallCompleted(It.Key(), &bFailed);
				if (bCompleted)
				{
					CompletedCalls.Add(It.Key());
				}
			}
			
			for (SteamAPICall_t CallHandle : CompletedCalls)
			{
				if (TFunction<void(SteamAPICall_t, bool)>* Callback = Entries.Find(CallHandle))
				{
					bool bFailed = false;
					SteamUtilsPtr->IsAPICallCompleted(CallHandle, &bFailed);
					(*Callback)(CallHandle, bFailed);
					Entries.Remove(CallHandle);
				}
			}
		}
	};

	FCallResultManager CallResults;

	void HandleUploadLeaderboardScore(SteamLeaderboard_t Leaderboard, int32 Score,
									  TFunction<void(const FGamingServiceResult&)> Callback)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: UploadLeaderboardScore begin "
					"(Leaderboard=%llu, Score=%d)"),
			   (uint64)Leaderboard, Score);
		SteamAPICall_t UploadHandle = SteamUserStats->UploadLeaderboardScore(
			Leaderboard, k_ELeaderboardUploadScoreMethodKeepBest, Score, nullptr, 0);
		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: UploadLeaderboardScore issued, "
					"CallHandle=%llu"),
			   UploadHandle);
		CallResults.Add<LeaderboardScoreUploaded_t>(
			UploadHandle,
			[Callback](const LeaderboardScoreUploaded_t& Up, bool bUploadFailure)
			{
				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: CallResult - "
							"UploadLeaderboardScore callback (SuccessFlag=%d, "
							"Score=%d, NewRank=%d, IOFail=%d)"),
					   (int32)Up.m_bSuccess, (int32)Up.m_nScore, (int32)Up.m_nGlobalRankNew, (int32)bUploadFailure);
				if (Callback)
				{
					Callback(FGamingServiceResult(!bUploadFailure && Up.m_bSuccess));
				}
			});
	}

	void HandleDownloadLeaderboardEntries(SteamLeaderboard_t Leaderboard, const FString& LeaderboardId, int32 Limit,
										  int32 ContinuationToken, TFunction<void(const FLeaderboardResult&)> Callback)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: DownloadLeaderboardEntries begin "
					"(Leaderboard=%llu, Start=%d, Limit=%d)"),
			   (uint64)Leaderboard, ContinuationToken, Limit);

		SteamAPICall_t DownloadHandle = SteamUserStats->DownloadLeaderboardEntries(
			Leaderboard, k_ELeaderboardDataRequestGlobal, ContinuationToken, ContinuationToken + Limit);

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: DownloadLeaderboardEntries issued, "
					"CallHandle=%llu"),
			   DownloadHandle);

		CallResults.Add<LeaderboardScoresDownloaded_t>(
			DownloadHandle,
			[this, LeaderboardId, Limit, ContinuationToken, Callback](const LeaderboardScoresDownloaded_t& Dl,
																	  bool bDownloadFailure)
			{
				if (bDownloadFailure)
				{
					if (Callback)
					{
						Callback(FLeaderboardResult(false, LeaderboardId, TArray<FLeaderboardEntry>(), 0, -1, 0));
					}
					return;
				}
				
				TArray<FLeaderboardEntry> Entries;
				int32 EntryCount = FMath::Min((int32)Dl.m_cEntryCount, Limit);

				for (int32 i = 0; i < EntryCount; ++i)
				{
					LeaderboardEntry_t Entry;
					if (!SteamUserStats->GetDownloadedLeaderboardEntry(Dl.m_hSteamLeaderboardEntries, i, &Entry,
																	   nullptr, 0))
					{
						UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Failed to get entry %d"), i);
						continue;
					}

					FLeaderboardEntry GameEntry;
					GameEntry.UserId = FString::Printf(TEXT("%llu"), Entry.m_steamIDUser.ConvertToUint64());
					GameEntry.Score = Entry.m_nScore;
					GameEntry.Rank = Entry.m_nGlobalRank;
					
					if (SteamFriends)
					{
						const char* Persona = SteamFriends->GetFriendPersonaName(Entry.m_steamIDUser);
						GameEntry.DisplayName = Persona ? UTF8_TO_TCHAR(Persona) : TEXT("Unknown");
					}
					else
					{
						GameEntry.DisplayName = TEXT("Unknown");
					}

					UE_LOG(LogTemp, Log,
						   TEXT("SteamworksGamingService: Entry %d: %s (%s) Score=%d "
								"Rank=%d"),
						   i, *GameEntry.DisplayName, *GameEntry.UserId, GameEntry.Score, GameEntry.Rank);

					Entries.Add(GameEntry);
				}
				
				FLeaderboardResult Result(true, LeaderboardId, Entries, Dl.m_cEntryCount);
				if (Entries.Num() > 0)
				{
					Result.ContinuationToken = ContinuationToken + Entries.Num() + 1;
				}
				else
				{
					Result.ContinuationToken = -1;
				}

				UE_LOG(LogTemp, Log,
					   TEXT("SteamworksGamingService: Completed - Total=%d, "
							"Returned=%d, NextToken=%d"),
					   (int32)Dl.m_cEntryCount, Entries.Num(), Result.ContinuationToken);

				if (Callback)
				{
					Callback(Result);
				}
			});
	}

	void InitializeSteamworks()
	{
		SteamErrMsg ErrMsg;
		ESteamAPIInitResult Result = SteamAPI_InitEx(&ErrMsg);
		if (Result != k_ESteamAPIInitResult_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Steam init failed (%d): %hs"), (int32)Result, ErrMsg);
			return;
		}

		SteamUserStats = ::SteamUserStats();
		SteamUser = ::SteamUser();
		SteamUtils = ::SteamUtils();
		SteamFriends = ::SteamFriends();
		SteamRemoteStorage = ::SteamRemoteStorage();
		SteamMatchmaking = ::SteamMatchmaking();
		SteamApps = ::SteamApps();

		if (!SteamUserStats || !SteamUser || !SteamUtils || !SteamFriends || !SteamRemoteStorage || !SteamMatchmaking || !SteamApps)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to get Steam interfaces"));
			SteamAPI_Shutdown();
			return;
		}

		CallResults.Initialize(SteamUtils);

		bIsInLobby = false;
		bIsLobbyHost = false;
		CurrentLobbyId = CSteamID();

		if (SteamUser->BLoggedOn())
		{
			bIsLoggedIn = true;
			CSteamID SteamID = SteamUser->GetSteamID();
			UserId = FString::Printf(TEXT("%llu"), SteamID.ConvertToUint64());

			const char* PersonaName = SteamFriends->GetPersonaName();
			DisplayName = PersonaName ? UTF8_TO_TCHAR(PersonaName) : TEXT("Steam User");

			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: User logged in: %s (ID: %s)"), *DisplayName, *UserId);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: User not logged in to Steam"));
		}

		bIsInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steamworks initialized successfully"));
	}

	void ShutdownSteamworks()
	{
		if (bIsInitialized)
		{
			FScopeLock Lock(&CallbackCriticalSection);
			SteamAPI_Shutdown();
			bIsInitialized = false;
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steamworks shutdown"));
		}
	}
};

FSteamworksGamingService::FSteamworksGamingService() { Impl = MakeUnique<FSteamworksGamingServiceImpl>(this); }

FSteamworksGamingService::~FSteamworksGamingService() {}

void FSteamworksGamingService::InitializePlatform()
{
	FString AppId;
	if (GConfig->GetString(TEXT("GamingServices.Steamworks"), TEXT("AppId"), AppId, GGameIni))
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("SteamAppId"), *AppId);
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Set SteamAppId=%s from config"), *AppId);
	}

	Impl->InitializeSteamworks();
}

void FSteamworksGamingService::DestroyPlatform()
{
	Impl->Shutdown();
	Impl->ShutdownSteamworks();
}

void FSteamworksGamingService::Login(const FGamingServiceLoginParams& Params, TFunction<void(const FGamingServiceResult&)> Callback)
{
	Callback(FGamingServiceResult(true));
}

void FSteamworksGamingService::UnlockAchievement(const FString& AchievementId, TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->UnlockAchievement(AchievementId, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
{
	Impl->QueryAchievements(MoveTemp(Callback));
}

void FSteamworksGamingService::ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback)
{
	Impl->ListEntitlements(MoveTemp(Callback));
}

void FSteamworksGamingService::HasEntitlement(const FEntitlementDefinition& Definition,
                                              TFunction<void(const FHasEntitlementResult&)> Callback)
{
	Impl->HasEntitlement(Definition, MoveTemp(Callback));
}

void FSteamworksGamingService::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score, TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->WriteLeaderboardScore(LeaderboardId, Score, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken, TFunction<void(const FLeaderboardResult&)> Callback)
{
	Impl->QueryLeaderboardPage(LeaderboardId, Limit, ContinuationToken, MoveTemp(Callback));
}

void FSteamworksGamingService::IngestStat(const FString& StatName, int32 Amount, TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->IngestStat(StatName, Amount, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryStat(const FString& StatName, TFunction<void(const FStatQueryResult&)> Callback)
{
	Impl->QueryStat(StatName, MoveTemp(Callback));
}

void FSteamworksGamingService::WriteFile(const FString& FilePath, const TArray<uint8>& Data, TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->WriteFile(FilePath, Data, MoveTemp(Callback));
}

void FSteamworksGamingService::ReadFile(const FString& FilePath, TFunction<void(const FFileReadResult&)> Callback)
{
	Impl->ReadFile(FilePath, MoveTemp(Callback));
}

void FSteamworksGamingService::DeleteFile(const FString& FilePath, TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->DeleteFile(FilePath, MoveTemp(Callback));
}

void FSteamworksGamingService::ListFiles(const FString& DirectoryPath, TFunction<void(const FFilesListResult&)> Callback)
{
	Impl->ListFiles(DirectoryPath, MoveTemp(Callback));
}

void FSteamworksGamingService::Tick() { Impl->Tick(); }

bool FSteamworksGamingService::IsInitialized() const { return Impl->IsInitialized(); }

bool FSteamworksGamingService::IsLoggedIn() const { return Impl->IsLoggedIn(); }

bool FSteamworksGamingService::NeedsLogin() const { return Impl->NeedsLogin(); }

FString FSteamworksGamingService::GetUserId() const { return Impl->GetUserId(); }

FString FSteamworksGamingService::GetDisplayName() const { return Impl->GetDisplayName(); }

UTexture2D* FSteamworksGamingService::GetAvatar() const { return Impl->GetAvatar(); }

UTexture2D* FSteamworksGamingService::GetAvatarByUserId(const FString& UserId) const { return Impl->GetAvatarByUserId(UserId); }

bool FSteamworksGamingService::IsSteamRunning() const { return Impl->IsSteamRunning(); }

bool FSteamworksGamingService::IsSteamOverlayEnabled() const { return Impl->IsSteamOverlayEnabled(); }

void FSteamworksGamingService::CreateSession(const FSessionSettings& Settings, TFunction<void(const FSessionCreateResult&)> Callback)
{
	Impl->CreateSession(Settings, MoveTemp(Callback));
}

void FSteamworksGamingService::FindSessions(const FSessionSearchFilter& Filter, TFunction<void(const FSessionSearchResult&)> Callback)
{
	Impl->FindSessions(Filter, MoveTemp(Callback));
}

void FSteamworksGamingService::JoinSession(const FSessionJoinHandle& JoinHandle, TFunction<void(const FSessionJoinResult&)> Callback)
{
	Impl->JoinSession(JoinHandle, MoveTemp(Callback));
}

void FSteamworksGamingService::LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->LeaveSession(MoveTemp(Callback));
}

void FSteamworksGamingService::DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->DestroySession(MoveTemp(Callback));
}

void FSteamworksGamingService::UpdateSession(const FSessionSettings& Settings, TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->UpdateSession(Settings, MoveTemp(Callback));
}

void FSteamworksGamingService::LockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->LockLobby(MoveTemp(Callback));
}

void FSteamworksGamingService::UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->UnlockLobby(MoveTemp(Callback));
}

void FSteamworksGamingService::GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
{
	Impl->GetCurrentSession(MoveTemp(Callback));
}

void FSteamworksGamingService::ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->ShowInviteFriendsDialog(MoveTemp(Callback));
}

FString FSteamworksGamingService::GetSessionConnectionString() const
{
	return Impl->GetSessionConnectionString();
}

#endif

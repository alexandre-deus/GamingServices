#ifdef USE_STEAMWORKS

#include "SteamworksGamingService.h"
#include "Engine/World.h"
#include "HAL/CriticalSection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "steam/steam_api.h"

class FSteamworksGamingService::FSteamworksGamingServiceImpl
{
public:
	FSteamworksGamingServiceImpl(FSteamworksGamingService* InOwner) :
		Owner(InOwner), bIsInitialized(false), bIsLoggedIn(false)
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

		ShutdownSteamworks();

		bIsInitialized = false;
		bIsLoggedIn = false;
		UserId.Empty();
		DisplayName.Empty();

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
				Callback(FFileReadResult::Failure(FilePath));
			}
			return;
		}

		int32 FileSize = SteamRemoteStorage->GetFileSize(FilePathUTF8);
		if (FileSize <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Invalid file size for: %s"), *FilePath);
			if (Callback)
			{
				Callback(FFileReadResult::Failure(FilePath));
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
				Callback(FFileReadResult::Failure(FilePath));
			}
			return;
		}

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: File read from cloud successfully: "
					"%s (%d bytes)"),
			   *FilePath, FileSize);
		if (Callback)
		{
			Callback(FFileReadResult::Success(FilePath, FileData));
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
		}
	}

	bool IsInitialized() const { return bIsInitialized; }
	bool IsConnected() const { return bIsInitialized; }
	bool IsLoggedIn() const { return bIsLoggedIn; }
	bool NeedsLogin() const { return false; }
	const FString& GetUserId() const { return UserId; }
	const FString& GetDisplayName() const { return DisplayName; }

	bool IsSteamRunning() const { return SteamAPI_IsSteamRunning(); }

	bool IsSteamOverlayEnabled() const { return SteamUtils ? SteamUtils->IsOverlayEnabled() : false; }

private:
	FSteamworksGamingService* Owner;

	bool bIsInitialized;
	bool bIsLoggedIn;
	FString UserId;
	FString DisplayName;

	ISteamUserStats* SteamUserStats;
	ISteamUser* SteamUser;
	ISteamUtils* SteamUtils;
	ISteamFriends* SteamFriends;
	ISteamRemoteStorage* SteamRemoteStorage;

	FCriticalSection CallbackCriticalSection;

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
			Leaderboard, k_ELeaderboardDataRequestGlobal, ContinuationToken, Limit);

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
				Result.ContinuationToken = ContinuationToken + Entries.Num();

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
		if (!SteamAPI_Init())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to initialize Steamworks API"));
			return;
		}

		SteamUserStats = ::SteamUserStats();
		SteamUser = ::SteamUser();
		SteamUtils = ::SteamUtils();
		SteamFriends = ::SteamFriends();
		SteamRemoteStorage = ::SteamRemoteStorage();

		if (!SteamUserStats || !SteamUser || !SteamUtils || !SteamFriends || !SteamRemoteStorage)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to get Steam interfaces"));
			SteamAPI_Shutdown();
			return;
		}

		CallResults.Initialize(SteamUtils);

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
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steamworks shutdown"));
		}
	}
};

FSteamworksGamingService::FSteamworksGamingService() { Impl = MakeUnique<FSteamworksGamingServiceImpl>(this); }

FSteamworksGamingService::~FSteamworksGamingService() {}

bool FSteamworksGamingService::Connect(const FGamingServiceConnectParams& Params)
{
	return Impl->Connect(Params.Steamworks);
}

void FSteamworksGamingService::Shutdown() { Impl->Shutdown(); }

void FSteamworksGamingService::Login(const FGamingServiceLoginParams& Params,
									 TFunction<void(const FGamingServiceResult&)> Callback)
{
	Callback(FGamingServiceResult(true));
}

void FSteamworksGamingService::UnlockAchievement(const FString& AchievementId,
												 TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->UnlockAchievement(AchievementId, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
{
	Impl->QueryAchievements(MoveTemp(Callback));
}

void FSteamworksGamingService::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
													 TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->WriteLeaderboardScore(LeaderboardId, Score, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
													TFunction<void(const FLeaderboardResult&)> Callback)
{
	Impl->QueryLeaderboardPage(LeaderboardId, Limit, ContinuationToken, MoveTemp(Callback));
}

void FSteamworksGamingService::IngestStat(const FString& StatName, int32 Amount,
										  TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->IngestStat(StatName, Amount, MoveTemp(Callback));
}

void FSteamworksGamingService::QueryStat(const FString& StatName, TFunction<void(const FStatQueryResult&)> Callback)
{
	Impl->QueryStat(StatName, MoveTemp(Callback));
}

void FSteamworksGamingService::WriteFile(const FString& FilePath, const TArray<uint8>& Data,
										 TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->WriteFile(FilePath, Data, MoveTemp(Callback));
}

void FSteamworksGamingService::ReadFile(const FString& FilePath, TFunction<void(const FFileReadResult&)> Callback)
{
	Impl->ReadFile(FilePath, MoveTemp(Callback));
}

void FSteamworksGamingService::DeleteFile(const FString& FilePath,
										  TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->DeleteFile(FilePath, MoveTemp(Callback));
}

void FSteamworksGamingService::ListFiles(const FString& DirectoryPath,
										 TFunction<void(const FFilesListResult&)> Callback)
{
	Impl->ListFiles(DirectoryPath, MoveTemp(Callback));
}

void FSteamworksGamingService::Tick() { Impl->Tick(); }

bool FSteamworksGamingService::IsInitialized() const { return Impl->IsInitialized(); }

bool FSteamworksGamingService::IsLoggedIn() const { return Impl->IsLoggedIn(); }

bool FSteamworksGamingService::NeedsLogin() const { return Impl->NeedsLogin(); }

FString FSteamworksGamingService::GetUserId() const { return Impl->GetUserId(); }

FString FSteamworksGamingService::GetDisplayName() const { return Impl->GetDisplayName(); }

bool FSteamworksGamingService::IsSteamRunning() const { return Impl->IsSteamRunning(); }

bool FSteamworksGamingService::IsSteamOverlayEnabled() const { return Impl->IsSteamOverlayEnabled(); }

#endif

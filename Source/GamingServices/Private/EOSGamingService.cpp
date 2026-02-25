#ifdef USE_EOS

#include "EOSGamingService.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

#include "eos_sdk.h"
#include "eos_common.h"
#include "eos_auth.h"
#include "eos_achievements.h"
#include "eos_stats.h"
#include "eos_leaderboards.h"
#include "eos_connect.h"
#include "eos_logging.h"
#include "eos_playerdatastorage.h"
#include "eos_sessions.h"

template <typename TResult, typename TOwner>
struct TEOSCallbackContext
{
	TOwner* Service;
	TFunction<void(const TResult&)> Callback;

	static TEOSCallbackContext* Create(TOwner* InService, TFunction<void(const TResult&)> InCallback)
	{
		TEOSCallbackContext* Ctx = new TEOSCallbackContext{};
		Ctx->Service = InService;
		Ctx->Callback = MoveTemp(InCallback);
		return Ctx;
	}

	static void Complete(TEOSCallbackContext* Ctx, const TResult& Result)
	{
		if (Ctx->Callback)
		{
			Ctx->Callback(Result);
		}
		delete Ctx;
	}
};

using FAuthCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSGamingService>;
using FAchievementUnlockCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSGamingService>;
using FAchievementsQueryCallbackCtx = TEOSCallbackContext<FAchievementsQueryResult, FEOSGamingService>;
using FStatIngestCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSGamingService>;
using FStatQueryCallbackCtx = TEOSCallbackContext<FStatQueryResult, FEOSGamingService>;
using FLeaderboardCallbackCtx = TEOSCallbackContext<FLeaderboardResult, FEOSGamingService>;
using FAchievementDefinitionsCallbackCtx = TEOSCallbackContext<bool, FEOSGamingService>;
using FLeaderboardDefinitionsCallbackCtx = TEOSCallbackContext<bool, FEOSGamingService>;
using FFileStorageCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSGamingService>;
using FFilesListCallbackCtx = TEOSCallbackContext<FFilesListResult, FEOSGamingService>;
using FSessionCreateCallbackCtx = TEOSCallbackContext<FSessionCreateResult, FEOSGamingService>;
using FSessionSearchCallbackCtx = TEOSCallbackContext<FSessionSearchResult, FEOSGamingService>;
using FSessionJoinCallbackCtx = TEOSCallbackContext<FSessionJoinResult, FEOSGamingService>;
using FSessionUpdateCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSGamingService>;

struct FEOSSessionJoinHandle : public ISessionJoinHandle
{
	EOS_HSessionDetails Handle = nullptr;
	FString SessionName;
	
	FEOSSessionJoinHandle(EOS_HSessionDetails InHandle, const FString& InSessionName)
		: Handle(InHandle), SessionName(InSessionName) {}
	~FEOSSessionJoinHandle()
	{
		if (Handle)
		{
			EOS_SessionDetails_Release(Handle);
			Handle = nullptr;
		}
	}
};

class FEOSGamingService::FEOSGamingServiceImpl
{
public:
	static constexpr const TCHAR* CloudStorageDirectoryName = TEXT("EOSRemoteStorage");
	static constexpr const TCHAR* ManifestFileName = TEXT("manifest.json");

	FEOSGamingServiceImpl(FEOSGamingService* InOwner)
		: Owner(InOwner)
	{
	}

	~FEOSGamingServiceImpl()
	{
	}

	bool Connect(const FEOSInitOptions& EOSOpts)
	{
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting initialization..."));

		if (!InitializeEOSPlatform(EOSOpts))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to initialize EOS platform"));
			return false;
		}

		if (!PlatformHandle)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Initialization failed"));
			return false;
		}

		if (TempStoragePath.IsEmpty())
		{
			TempStoragePath = FPaths::ProjectSavedDir() / CloudStorageDirectoryName;
			IFileManager::Get().MakeDirectory(*TempStoragePath, true);
		}

		bIsInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform initialized successfully"));
		return true;
	}

	void Shutdown()
	{
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting shutdown..."));

		if (bIsLoggedIn && ProductUserId && PlayerDataStorageHandle)
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Syncing to cloud before shutdown..."));

			bool bSyncCompleted = false;
			bool bSyncSuccess = false;

			SyncToCloud([&bSyncCompleted, &bSyncSuccess](const FGamingServiceResult& Result)
			{
				bSyncSuccess = Result.bSuccess;
				bSyncCompleted = true;
				if (Result.bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Shutdown sync to cloud completed successfully"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Shutdown sync to cloud failed"));
				}
			});

			while (!bSyncCompleted)
			{
				if (PlatformHandle)
				{
					EOS_Platform_Tick(PlatformHandle);
				}
			}

			if (!bSyncCompleted)
			{
				UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Shutdown sync to cloud timed out"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Skipping cloud sync - not logged in or handles not valid"));
		}

		ShutdownEOSPlatform();

		bIsInitialized = false;
		bIsConnected = false;
		bIsLoggedIn = false;
		UserId.Empty();
		DisplayName.Empty();

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Shutdown completed"));
	}

	void UnlockAchievement(const FString& AchievementId,
	                       TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && AchievementsHandle,
		       TEXT("EOSGamingService: UnlockAchievement called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Unlocking achievement: %s"), *AchievementId);

		EOS_Achievements_UnlockAchievementsOptions UnlockOptions = {};
		UnlockOptions.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST;
		UnlockOptions.UserId = ProductUserId;

		const char* AchievementIds[] = {TCHAR_TO_UTF8(*AchievementId)};
		UnlockOptions.AchievementIds = AchievementIds;
		UnlockOptions.AchievementsCount = 1;

		auto* Ctx = FAchievementUnlockCallbackCtx::Create(Owner, MoveTemp(Callback));
		EOS_Achievements_UnlockAchievements(
			AchievementsHandle,
			&UnlockOptions,
			Ctx,
			[](const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAchievementUnlockCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);
				FGamingServiceResult Result((Data->ResultCode == EOS_EResult::EOS_Success));
				if (Result.bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Achievement unlocked successfully"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to unlock achievement: %d"),
					       (int32)Data->ResultCode);
				}
				FAchievementUnlockCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && AchievementsHandle,
		       TEXT("EOSGamingService: QueryAchievements called when service not ready"));

		auto* Ctx = FAchievementsQueryCallbackCtx::Create(Owner, MoveTemp(Callback));

		EOS_Achievements_QueryPlayerAchievementsOptions PlayerOpts = {};
		PlayerOpts.ApiVersion = EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST;
		PlayerOpts.LocalUserId = ProductUserId;
		PlayerOpts.TargetUserId = ProductUserId;

		EOS_Achievements_QueryPlayerAchievements(
			AchievementsHandle,
			&PlayerOpts,
			Ctx,
			[](const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAchievementsQueryCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);

				FAchievementsQueryResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!Result.bSuccess)
				{
					FAchievementsQueryCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				TArray<FGameAchievement> Achievements;
				for (const auto& DefinitionPair : Service->Impl->AchievementDefinitions)
				{
					EOS_Achievements_DefinitionV2* Definition = DefinitionPair.Value;
					if (Definition)
					{
						FGameAchievement GameAchievement;
						Service->Impl->ConvertEOSAchievementToGameAchievement(Definition, nullptr, GameAchievement);
						Achievements.Add(GameAchievement);
					}
				}

				Result.Achievements = Achievements;
				FAchievementsQueryCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                           TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && StatsHandle,
		       TEXT("EOSGamingService: WriteLeaderboardScore called when service not ready"));
		checkf(bDefinitionsLoaded,
		       TEXT("EOSGamingService: WriteLeaderboardScore called before definitions are loaded"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Writing leaderboard score: %d to %s"), Score, *LeaderboardId);

		EOS_Leaderboards_Definition* LeaderboardDef = nullptr;
		if (LeaderboardDefinitions.Contains(LeaderboardId))
		{
			LeaderboardDef = LeaderboardDefinitions[LeaderboardId];
		}

		if (!LeaderboardDef)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Leaderboard definition not found for: %s"), *LeaderboardId);
			Callback(FGamingServiceResult(false));
			return;
		}

		FString StatName = UTF8_TO_TCHAR(LeaderboardDef->StatName);
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Found stat name '%s' for leaderboard '%s'"), *StatName,
		       *LeaderboardId);

		IngestStat(StatName, Score, [Callback](const FGamingServiceResult& StatResult)
		{
			Callback(StatResult);
		});
	}

	void IngestStat(const FString& StatName, int32 Amount,
	                TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && StatsHandle,
		       TEXT("EOSGamingService: IngestStat called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Ingesting %d into stat %s"), Amount, *StatName);

		EOS_Stats_IngestStatOptions IngestOptions = {};
		IngestOptions.ApiVersion = EOS_STATS_INGESTSTAT_API_LATEST;
		IngestOptions.LocalUserId = ProductUserId;
		IngestOptions.TargetUserId = ProductUserId;
		IngestOptions.StatsCount = 1;

		EOS_Stats_IngestData Stat = {};
		Stat.ApiVersion = EOS_STATS_INGESTDATA_API_LATEST;
		Stat.StatName = TCHAR_TO_UTF8(*StatName);
		Stat.IngestAmount = Amount;

		IngestOptions.Stats = &Stat;

		struct FStatIngestCtx : FStatIngestCallbackCtx
		{
			FString StatName;
			int32 Amount = 0;
		};
		auto* Ctx = new FStatIngestCtx{};
		Ctx->Service = Owner;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->StatName = StatName;
		Ctx->Amount = Amount;

		EOS_Stats_IngestStat(
			StatsHandle,
			&IngestOptions,
			Ctx,
			[](const EOS_Stats_IngestStatCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FStatIngestCtx*>(Data->ClientData);
				FGamingServiceResult Result((Data->ResultCode == EOS_EResult::EOS_Success));
				FStatIngestCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void QueryStat(const FString& StatName,
	               TFunction<void(const FStatQueryResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && StatsHandle && ProductUserId != nullptr,
		       TEXT("EOSGamingService: QueryStat called when service not ready"));

		struct FStatQueryCtx : FStatQueryCallbackCtx
		{
			FString StatName;
		};
		auto* Ctx = new FStatQueryCtx{};
		Ctx->Service = Owner;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->StatName = StatName;

		EOS_Stats_QueryStatsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_STATS_QUERYSTATS_API_LATEST;
		QueryOptions.LocalUserId = ProductUserId;
		QueryOptions.TargetUserId = ProductUserId;
		QueryOptions.StartTime = EOS_STATS_TIME_UNDEFINED;
		QueryOptions.EndTime = EOS_STATS_TIME_UNDEFINED;

		EOS_Stats_QueryStats(
			StatsHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_Stats_OnQueryStatsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FStatQueryCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					FStatQueryResult Result;
					Result.bSuccess = false;
					Result.StatName = LocalCtx->StatName;
					FStatQueryCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				EOS_Stats_GetStatCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_STATS_GETSTATCOUNT_API_LATEST;
				CountOptions.TargetUserId = Service->Impl->ProductUserId;
				uint32_t Count = EOS_Stats_GetStatsCount(Service->Impl->StatsHandle, &CountOptions);

				int32 FoundValue = 0;
				bool bFound = false;
				for (uint32_t i = 0; i < Count; ++i)
				{
					EOS_Stats_CopyStatByIndexOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_STATS_COPYSTATBYINDEX_API_LATEST;
					CopyOptions.TargetUserId = Service->Impl->ProductUserId;
					CopyOptions.StatIndex = i;

					EOS_Stats_Stat* Stat = nullptr;
					if (EOS_Stats_CopyStatByIndex(Service->Impl->StatsHandle, &CopyOptions, &Stat) ==
						EOS_EResult::EOS_Success && Stat)
					{
						if (LocalCtx->StatName == UTF8_TO_TCHAR(Stat->Name))
						{
							FoundValue = Stat->Value;
							bFound = true;
						}
						EOS_Stats_Stat_Release(Stat);
					}
				}

				FStatQueryResult Result;
				if (bFound)
				{
					Result = FStatQueryResult::Make(LocalCtx->StatName, FoundValue);
				}
				else
				{
					Result.bSuccess = false;
					Result.StatName = LocalCtx->StatName;
				}
				FStatQueryCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
	                          TFunction<void(const FLeaderboardResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && LeaderboardsHandle,
		       TEXT("EOSGamingService: QueryLeaderboardPage called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Querying leaderboard page: %s (Continuation: %d, Limit: %d)"),
		       *LeaderboardId, ContinuationToken, Limit);

		EOS_Leaderboards_QueryLeaderboardRanksOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST;
		QueryOptions.LeaderboardId = TCHAR_TO_UTF8(*LeaderboardId);
		QueryOptions.LocalUserId = ProductUserId;

		struct FLeaderboardQueryCtx : FLeaderboardCallbackCtx
		{
			FString LeaderboardId;
			int32 Limit;
			int32 ContinuationToken;
		};
		auto* Ctx = new FLeaderboardQueryCtx{};
		Ctx->Service = Owner;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->LeaderboardId = LeaderboardId;
		Ctx->Limit = Limit;
		Ctx->ContinuationToken = ContinuationToken;

		EOS_Leaderboards_QueryLeaderboardRanks(
			LeaderboardsHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FLeaderboardQueryCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);
				FLeaderboardResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				Result.LeaderboardId = LocalCtx->LeaderboardId;
				if (!Result.bSuccess)
				{
					FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
					return;
				}
				EOS_Leaderboards_GetLeaderboardRecordCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST;
				uint32_t RecordCount = EOS_Leaderboards_GetLeaderboardRecordCount(
					Service->Impl->LeaderboardsHandle, &CountOptions);

				TArray<FLeaderboardEntry> Entries;
				uint32_t StartIndex = FMath::Max(0, LocalCtx->ContinuationToken);
				uint32_t EndIndex = FMath::Min(RecordCount, StartIndex + LocalCtx->Limit);
				for (uint32_t i = StartIndex; i < EndIndex; ++i)
				{
					EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST;
					CopyOptions.LeaderboardRecordIndex = i;

					EOS_Leaderboards_LeaderboardRecord* Record = nullptr;
					if (EOS_Leaderboards_CopyLeaderboardRecordByIndex(
						Service->Impl->LeaderboardsHandle, &CopyOptions, &Record) == EOS_EResult::EOS_Success && Record)
					{
						FLeaderboardEntry Entry;
						Service->Impl->ConvertEOSLeaderboardRecordToEntry(Record, Entry);
						Entries.Add(Entry);
						EOS_Leaderboards_LeaderboardRecord_Release(Record);
					}
				}
				Result.Entries = Entries;
				Result.TotalEntries = RecordCount;
				Result.ContinuationToken = EndIndex < RecordCount ? EndIndex : -1;
				FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void WriteFile(const FString& FilePath, const TArray<uint8>& Data,
	               TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized, TEXT("EOSGamingService: WriteFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Writing file to cloud storage: %s (%d bytes)"), *FilePath, Data.Num());

		FString FullPath = GetFullLocalPath(FilePath);

		if (!FFileHelper::SaveArrayToFile(Data, *FullPath))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to write to cloud storage: %s"), *FullPath);
			Callback(FGamingServiceResult(false));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File written to cloud storage successfully"));
		Callback(FGamingServiceResult(true));
	}

	void ReadFile(const FString& FilePath,
	              TFunction<void(const FFileReadResult&)> Callback)
	{
		checkf(bIsInitialized, TEXT("EOSGamingService: ReadFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Reading file from cloud storage: %s"), *FilePath);

		FString FullPath = GetFullLocalPath(FilePath);

		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FullPath))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to read from cloud storage: %s"), *FullPath);
			Callback(FFileReadResult(false, FilePath));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File read from cloud storage successfully (%d bytes)"), FileData.Num());
		Callback(FFileReadResult(true, FilePath, FileData));
	}

	void DeleteFile(const FString& FilePath,
	                TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized, TEXT("EOSGamingService: DeleteFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Deleting file from local storage: %s"), *FilePath);

		FString FullPath = GetFullLocalPath(FilePath);

		if (IFileManager::Get().Delete(*FullPath))
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File deleted from local storage successfully"));
			Callback(FGamingServiceResult(true));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to delete file from local storage: %s"), *FullPath);
			Callback(FGamingServiceResult(false));
		}
	}

	void ListFiles(const FString& DirectoryPath,
	               TFunction<void(const FFilesListResult&)> Callback)
	{
		checkf(bIsInitialized, TEXT("EOSGamingService: ListFiles called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Listing files in local storage directory: %s"), *DirectoryPath);

		FString FullDirectoryPath = TempStoragePath.IsEmpty() 
			? (FPaths::ProjectSavedDir() / CloudStorageDirectoryName / DirectoryPath)
			: (TempStoragePath / DirectoryPath);

		FFilesListResult Result;
		Result.bSuccess = true;

		if (!FPaths::DirectoryExists(FullDirectoryPath))
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Directory does not exist: %s"), *FullDirectoryPath);
			Callback(Result);
			return;
		}

		TArray<FString> FoundFiles;
		IFileManager::Get().FindFiles(FoundFiles, *(FullDirectoryPath / TEXT("*")), true, false);

		for (const FString& File : FoundFiles)
		{
			FString FilePath = DirectoryPath.IsEmpty() ? File : (DirectoryPath / File);
			FString FullFilePath = FullDirectoryPath / File;

			FFileBlobData FileData;
			FileData.FilePath = FilePath;

			const int64 FileSize = IFileManager::Get().FileSize(*FullFilePath);
			FileData.Size = FileSize > 0 ? FileSize : 0;

			FileData.LastModified = IFileManager::Get().GetTimeStamp(*FullFilePath);

			Result.Files.Add(FileData);
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Found %d files in local storage"), Result.Files.Num());
		Callback(Result);
	}

	void Tick()
	{
		if (PlatformHandle)
		{
			EOS_Platform_Tick(PlatformHandle);
		}
	}

	bool IsInitialized() const { return bIsInitialized; }
	bool IsConnected() const { return bIsConnected; }
	bool IsLoggedIn() const { return bIsLoggedIn; }
	bool NeedsLogin() const { return true; }
	const FString& GetUserId() const { return UserId; }
	const FString& GetDisplayName() const { return DisplayName; }

	void AuthLogin(const FGamingServiceLoginParams& Params, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(AuthHandle, TEXT("EOSGamingService: AuthLogin called when AuthHandle is not initialized"));

		EOS_Auth_LoginOptions LoginOptions = {};
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;

		EOS_Auth_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

		switch (Params.EOS.Method)
		{
		case EEOSLoginMethod::PersistentAuth:
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
			break;
		case EEOSLoginMethod::AccountPortal:
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
			break;
		case EEOSLoginMethod::DeviceCode:
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_DeviceCode;
			break;
		case EEOSLoginMethod::Developer:
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;
			if (!Params.EOS.DeveloperHost.IsEmpty())
			{
				Credentials.Id = TCHAR_TO_UTF8(*Params.EOS.DeveloperHost);
			}
			if (!Params.EOS.DeveloperCredentialName.IsEmpty())
			{
				Credentials.Token = TCHAR_TO_UTF8(*Params.EOS.DeveloperCredentialName);
			}
			break;
		default:
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
			break;
		}

		LoginOptions.Credentials = &Credentials;

		auto* Ctx = FAuthCallbackCtx::Create(Owner, MoveTemp(Callback));

		EOS_Auth_Login(
			AuthHandle,
			&LoginOptions,
			Ctx,
			[](const EOS_Auth_LoginCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAuthCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				checkf(Service && Service->Impl,
				       TEXT("EOSGamingService: Auth login failed because Service or Service->Impl is not initialized"));
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Auth login failed: %d"), (int32)Data->ResultCode);
					FAuthCallbackCtx::Complete(LocalCtx, FGamingServiceResult(false));
					return;
				}

				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Auth login successful"));
				Service->Impl->ConnectLogin(Data->LocalUserId, [LocalCtx](const FGamingServiceResult& ConnectResult)
				{
					FAuthCallbackCtx::Complete(LocalCtx, ConnectResult);
				});
			}
		);
	}

	void ConnectLogin(EOS_EpicAccountId EpicAccountId, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(AuthHandle && ConnectHandle,
		       TEXT("EOSGamingService: ConnectLogin called when handles are not initialized"));

		EOS_Auth_Token* AuthToken = nullptr;
		EOS_Auth_CopyUserAuthTokenOptions CopyOpts = {};
		CopyOpts.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;
		EOS_EResult CopyRes = EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyOpts, EpicAccountId, &AuthToken);
		if (CopyRes != EOS_EResult::EOS_Success || AuthToken == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to copy user auth token: %d"), (int32)CopyRes);
			Callback(FGamingServiceResult(false));
			return;
		}

		EOS_Connect_Credentials ConnectCreds = {};
		ConnectCreds.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
		ConnectCreds.Token = AuthToken->AccessToken;
		ConnectCreds.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC;

		EOS_Connect_LoginOptions ConnLoginOpts = {};
		ConnLoginOpts.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
		ConnLoginOpts.Credentials = &ConnectCreds;
		ConnLoginOpts.UserLoginInfo = nullptr;

		auto* Ctx = FAuthCallbackCtx::Create(Owner, MoveTemp(Callback));

		EOS_Connect_Login(
			ConnectHandle,
			&ConnLoginOpts,
			Ctx,
			[](const EOS_Connect_LoginCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAuthCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				checkf(Service && Service->Impl,
				       TEXT("EOSGamingService: Connect login failed because Service or Service->Impl is not initialized"
				       ));

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Connect login successful"));
					Service->Impl->CompleteAuthentication(Data->LocalUserId, LocalCtx);
					return;
				}

				if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && Data->ContinuanceToken)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: User not found, creating new user"));
					Service->Impl->CreateUser(LocalCtx, Data->ContinuanceToken);
					return;
				}

				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Connect login failed: %d"), (int32)Data->ResultCode);
				FAuthCallbackCtx::Complete(LocalCtx, FGamingServiceResult(false));
			}
		);
	}

	void CreateSession(const FSessionSettings& Settings, TFunction<void(const FSessionCreateResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SessionsHandle,
		       TEXT("EOSGamingService: CreateSession called when service not ready"));

		if (bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Already in a session, destroying old session first"));
			DestroySession([this, Settings, Callback](const FGamingServiceResult& Result)
			{
				if (!Result.bSuccess)
				{
					FSessionCreateResult ErrorResult;
					ErrorResult.bSuccess = false;
					Callback(ErrorResult);
					return;
				}
				CreateSession(Settings, Callback);
			});
			return;
		}
		
		EOS_Sessions_CreateSessionModificationOptions CreateOptions = {};
		CreateOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
		CreateOptions.SessionName = TCHAR_TO_UTF8(*Settings.SessionName);
		CreateOptions.BucketId = TCHAR_TO_UTF8(TEXT("GameSessions"));
		CreateOptions.MaxPlayers = Settings.MaxPlayers;
		CreateOptions.LocalUserId = ProductUserId;

		EOS_HSessionModification SessionModHandle = nullptr;
		EOS_EResult CreateModResult = EOS_Sessions_CreateSessionModification(SessionsHandle, &CreateOptions, &SessionModHandle);
		
		if (CreateModResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session modification: %d"), (int32)CreateModResult);
			FSessionCreateResult ErrorResult;
			ErrorResult.bSuccess = false;
			Callback(ErrorResult);
			return;
		}

		EOS_SessionModification_SetPermissionLevelOptions PermissionOptions = {};
		PermissionOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
		if (Settings.Privacy == ESessionPrivacy::Public)
			PermissionOptions.PermissionLevel = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_PublicAdvertised;
		else if (Settings.Privacy == ESessionPrivacy::FriendsOnly)
			PermissionOptions.PermissionLevel = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly;
		else
			PermissionOptions.PermissionLevel = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly;

		EOS_SessionModification_SetPermissionLevel(SessionModHandle, &PermissionOptions);

		EOS_SessionModification_SetJoinInProgressAllowedOptions JoinIPOptions = {};
		JoinIPOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST;
		JoinIPOptions.bAllowJoinInProgress = Settings.bAllowJoinInProgress ? EOS_TRUE : EOS_FALSE;
		EOS_SessionModification_SetJoinInProgressAllowed(SessionModHandle, &JoinIPOptions);

		for (const FSessionAttribute& Attr : Settings.CustomAttributes)
		{
			EOS_SessionModification_AddAttributeOptions AttrOptions = {};
			AttrOptions.ApiVersion = EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST;

			EOS_Sessions_AttributeData AttributeData = {};
			AttributeData.ApiVersion = EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST;
			
			FTCHARToUTF8 KeyConverter(*Attr.Key);
			FTCHARToUTF8 ValueConverter(*Attr.Value);
			
			AttributeData.Key = KeyConverter.Get();
			AttributeData.ValueType = EOS_ESessionAttributeType::EOS_SAT_String;
			AttributeData.Value.AsUtf8 = ValueConverter.Get();

			AttrOptions.SessionAttribute = &AttributeData;
			AttrOptions.AdvertisementType = EOS_ESessionAttributeAdvertisementType::EOS_SAAT_Advertise;
			
			EOS_SessionModification_AddAttribute(SessionModHandle, &AttrOptions);
		}

		EOS_Sessions_UpdateSessionOptions UpdateOptions = {};
		UpdateOptions.ApiVersion = EOS_SESSIONS_UPDATESESSION_API_LATEST;
		UpdateOptions.SessionModificationHandle = SessionModHandle;

		struct CreateSessionContext
		{
			FSessionCreateCallbackCtx* CreateCallbackCtx;
			FSessionSettings Settings;
		};
		auto* CallbackContext = FSessionCreateCallbackCtx::Create(Owner, MoveTemp(Callback));
		auto* CreateContext = new CreateSessionContext{CallbackContext, Settings};
		EOS_Sessions_UpdateSession(
			SessionsHandle,
			&UpdateOptions,
			CreateContext,
			[](const EOS_Sessions_UpdateSessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<CreateSessionContext*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx->CreateCallbackCtx->Service;
				check(Service && Service->Impl);

				FSessionCreateResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				
				if (Result.bSuccess)
				{
					Service->Impl->bIsInSession = true;
					Service->Impl->bIsSessionHost = true;
					
					Result.SessionInfo.SessionName = LocalCtx->Settings.SessionName;
					Result.SessionInfo.HostUserId = Service->Impl->UserId;
					Result.SessionInfo.HostDisplayName = Service->Impl->DisplayName;
					
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Session created successfully: %s"), *LocalCtx->Settings.SessionName);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session: %d"), (int32)Data->ResultCode);
				}

				FSessionCreateCallbackCtx::Complete(LocalCtx->CreateCallbackCtx, Result);
				delete LocalCtx;
			}
		);

		//EOS_SessionModification_Release(SessionModHandle);
	}

	void FindSessions(const FSessionSearchFilter& Filter, TFunction<void(const FSessionSearchResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SessionsHandle,
		       TEXT("EOSGamingService: FindSessions called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Searching for sessions, max results: %d"), Filter.MaxResults);

		EOS_Sessions_CreateSessionSearchOptions SearchOptions = {};
		SearchOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST;
		SearchOptions.MaxSearchResults = Filter.MaxResults;

		EOS_HSessionSearch SearchHandle = nullptr;
		EOS_EResult CreateSearchResult = EOS_Sessions_CreateSessionSearch(SessionsHandle, &SearchOptions, &SearchHandle);
		
		if (CreateSearchResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session search: %d"), (int32)CreateSearchResult);
			FSessionSearchResult ErrorResult;
			ErrorResult.bSuccess = false;
			Callback(ErrorResult);
			return;
		}

		for (const FSessionAttribute& Attr : Filter.RequiredAttributes)
		{
			EOS_SessionSearch_SetParameterOptions ParamOptions = {};
			ParamOptions.ApiVersion = EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST;

			EOS_Sessions_AttributeData AttributeData = {};
			AttributeData.ApiVersion = EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST;
			
			FTCHARToUTF8 KeyConverter(*Attr.Key);
			FTCHARToUTF8 ValueConverter(*Attr.Value);
			
			AttributeData.Key = KeyConverter.Get();
			AttributeData.ValueType = EOS_ESessionAttributeType::EOS_SAT_String;
			AttributeData.Value.AsUtf8 = ValueConverter.Get();

			ParamOptions.Parameter = &AttributeData;
			ParamOptions.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
			
			EOS_SessionSearch_SetParameter(SearchHandle, &ParamOptions);
		}

		struct FSearchContext
		{
			FEOSGamingService* Service;
			TFunction<void(const FSessionSearchResult&)> Callback;
			EOS_HSessionSearch SearchHandle;
		};

		auto* SearchCtx = new FSearchContext{Owner, MoveTemp(Callback), SearchHandle};

		EOS_SessionSearch_FindOptions FindOptions = {};
		FindOptions.ApiVersion = EOS_SESSIONSEARCH_FIND_API_LATEST;
		FindOptions.LocalUserId = ProductUserId;

		EOS_SessionSearch_Find(
			SearchHandle,
			&FindOptions,
			SearchCtx,
			[](const EOS_SessionSearch_FindCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSearchContext*>(Data->ClientData);
				
				FSessionSearchResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					EOS_SessionSearch_GetSearchResultCountOptions CountOptions = {};
					CountOptions.ApiVersion = EOS_SESSIONSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
					uint32_t ResultCount = EOS_SessionSearch_GetSearchResultCount(LocalCtx->SearchHandle, &CountOptions);

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Found %d sessions"), ResultCount);

					for (uint32_t i = 0; i < ResultCount; i++)
					{
						EOS_SessionSearch_CopySearchResultByIndexOptions CopyOptions = {};
						CopyOptions.ApiVersion = EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
						CopyOptions.SessionIndex = i;

						EOS_SessionDetails_Info* SessionInfo = nullptr;
						EOS_HSessionDetails SessionDetails = nullptr;
						
						if (EOS_SessionSearch_CopySearchResultByIndex(LocalCtx->SearchHandle, &CopyOptions, &SessionDetails) == EOS_EResult::EOS_Success)
						{
							EOS_SessionDetails_CopyInfoOptions InfoOptions = {};
							InfoOptions.ApiVersion = EOS_SESSIONDETAILS_COPYINFO_API_LATEST;

							if (EOS_SessionDetails_CopyInfo(SessionDetails, &InfoOptions, &SessionInfo) == EOS_EResult::EOS_Success && SessionInfo)
							{
								FSessionInfo Session;
								Session.SessionName = SessionInfo->SessionId ? FString(SessionInfo->SessionId) : TEXT("");
								const int32 MaxPlayersVal = SessionInfo->Settings ? (int32)SessionInfo->Settings->NumPublicConnections : 0;
								Session.MaxPlayers = MaxPlayersVal;
								Session.CurrentPlayers = (int32)SessionInfo->NumOpenPublicConnections < MaxPlayersVal
									? MaxPlayersVal - (int32)SessionInfo->NumOpenPublicConnections
									: MaxPlayersVal;
								Session.AvailableSlots = (int32)SessionInfo->NumOpenPublicConnections;
								Session.JoinHandle.BackendHandle = MakeShared<FEOSSessionJoinHandle>(
									SessionDetails, Session.SessionName);
								SessionDetails = nullptr;

								Result.Sessions.Add(Session);

								EOS_SessionDetails_Info_Release(SessionInfo);
							}
							else if (SessionDetails)
							{
								EOS_SessionDetails_Release(SessionDetails);
							}
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Session search failed: %d"), (int32)Data->ResultCode);
				}

				LocalCtx->Callback(Result);
				EOS_SessionSearch_Release(LocalCtx->SearchHandle);
				delete LocalCtx;
			}
		);
	}

	void JoinSession(const FSessionJoinHandle& JoinHandle, TFunction<void(const FSessionJoinResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SessionsHandle,
		       TEXT("EOSGamingService: JoinSession called when service not ready"));

		if (!JoinHandle.BackendHandle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: JoinSession requires a session from FindSessions."));
			FSessionJoinResult FailResult;
			FailResult.bSuccess = false;
			Callback(FailResult);
			return;
		}

		if (bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Already in a session, leaving old session first"));
			LeaveSession([this, JoinHandle, Callback](const FGamingServiceResult& Result)
			{
				JoinSession(JoinHandle, Callback);
			});
			return;
		}


		auto* Ctx = FSessionJoinCallbackCtx::Create(Owner, MoveTemp(Callback));
		struct FJoinSessionPayload
		{
			FSessionJoinCallbackCtx* Ctx;
			TSharedPtr<FEOSSessionJoinHandle> JoinHandle;
		};
		auto* Payload = new FJoinSessionPayload{ Ctx, StaticCastSharedPtr<FEOSSessionJoinHandle>(JoinHandle.BackendHandle) };
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Joining session: %s"), *Payload->JoinHandle->SessionName);

		EOS_Sessions_JoinSessionOptions JoinOptions = {};
		JoinOptions.ApiVersion = EOS_SESSIONS_JOINSESSION_API_LATEST;
		JoinOptions.SessionName = TCHAR_TO_UTF8(*Payload->JoinHandle->SessionName);
		JoinOptions.SessionHandle = Payload->JoinHandle->Handle;
		JoinOptions.LocalUserId = ProductUserId;

		EOS_Sessions_JoinSession(
			SessionsHandle,
			&JoinOptions,
			Payload,
			[](const EOS_Sessions_JoinSessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalPayload = static_cast<FJoinSessionPayload*>(Data->ClientData);
				FSessionJoinCallbackCtx* LocalCtx = LocalPayload->Ctx;
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);

				FSessionJoinResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Service->Impl->bIsInSession = true;
					Service->Impl->bIsSessionHost = false;
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Successfully joined session"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to join session: %d"), (int32)Data->ResultCode);
				}

				if (LocalCtx->Callback)
				{
					LocalCtx->Callback(Result);
				}
				delete LocalCtx;
				delete LocalPayload;
			}
		);
	}

	void LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SessionsHandle,
		       TEXT("EOSGamingService: LeaveSession called when service not ready"));

		if (!bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Not in a session"));
			Callback(FGamingServiceResult(true));
			return;
		}

		if (bIsSessionHost)
		{
			DestroySession(Callback);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Leaving session"));

		auto* Ctx = FSessionUpdateCallbackCtx::Create(Owner, MoveTemp(Callback));

		EOS_Sessions_DestroySessionOptions DestroyOptions = {};
		DestroyOptions.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST;
		DestroyOptions.SessionName = TCHAR_TO_UTF8(*CurrentSessionName);

		EOS_Sessions_DestroySession(
			SessionsHandle,
			&DestroyOptions,
			Ctx,
			[](const EOS_Sessions_DestroySessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionUpdateCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);

				FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Service->Impl->bIsInSession = false;
					Service->Impl->CurrentSessionName.Empty();
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Successfully left session"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to leave session: %d"), (int32)Data->ResultCode);
				}

				FSessionUpdateCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SessionsHandle,
		       TEXT("EOSGamingService: DestroySession called when service not ready"));

		if (!bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Not in a session"));
			Callback(FGamingServiceResult(true));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Destroying session"));

		auto* Ctx = FSessionUpdateCallbackCtx::Create(Owner, MoveTemp(Callback));

		EOS_Sessions_DestroySessionOptions DestroyOptions = {};
		DestroyOptions.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST;
		DestroyOptions.SessionName = TCHAR_TO_UTF8(*CurrentSessionName);

		EOS_Sessions_DestroySession(
			SessionsHandle,
			&DestroyOptions,
			Ctx,
			[](const EOS_Sessions_DestroySessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionUpdateCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);

				FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Service->Impl->bIsInSession = false;
					Service->Impl->bIsSessionHost = false;
					Service->Impl->CurrentSessionName.Empty();
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Successfully destroyed session"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to destroy session: %d"), (int32)Data->ResultCode);
				}

				FSessionUpdateCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void UpdateSession(const FSessionSettings& Settings, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && SessionsHandle,
		       TEXT("EOSGamingService: UpdateSession called when service not ready"));

		if (!bIsInSession || !bIsSessionHost)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot update session - not hosting a session"));
			Callback(FGamingServiceResult(false));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Updating session"));

		EOS_Sessions_CreateSessionModificationOptions CreateOptions = {};
		CreateOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
		CreateOptions.SessionName = TCHAR_TO_UTF8(*CurrentSessionName);
		CreateOptions.BucketId = TCHAR_TO_UTF8(TEXT("GameSessions"));
		CreateOptions.MaxPlayers = Settings.MaxPlayers;

		EOS_HSessionModification SessionModHandle = nullptr;
		EOS_EResult CreateModResult = EOS_Sessions_CreateSessionModification(SessionsHandle, &CreateOptions, &SessionModHandle);
		
		if (CreateModResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session modification for update: %d"), (int32)CreateModResult);
			Callback(FGamingServiceResult(false));
			return;
		}

		EOS_SessionModification_SetJoinInProgressAllowedOptions JoinIPOptions = {};
		JoinIPOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST;
		JoinIPOptions.bAllowJoinInProgress = Settings.bAllowJoinInProgress ? EOS_TRUE : EOS_FALSE;
		EOS_SessionModification_SetJoinInProgressAllowed(SessionModHandle, &JoinIPOptions);

		EOS_Sessions_UpdateSessionOptions UpdateOptions = {};
		UpdateOptions.ApiVersion = EOS_SESSIONS_UPDATESESSION_API_LATEST;
		UpdateOptions.SessionModificationHandle = SessionModHandle;

		auto* Ctx = FSessionUpdateCallbackCtx::Create(Owner, MoveTemp(Callback));
		EOS_Sessions_UpdateSession(
			SessionsHandle,
			&UpdateOptions,
			Ctx,
			[](const EOS_Sessions_UpdateSessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionUpdateCallbackCtx*>(Data->ClientData);

				FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Session updated successfully"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to update session: %d"), (int32)Data->ResultCode);
				}

				FSessionUpdateCallbackCtx::Complete(LocalCtx, Result);
			}
		);

		EOS_SessionModification_Release(SessionModHandle);
	}

	void GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
	{
		FSessionInfo Info;
		
		if (bIsInSession)
		{
			Info.SessionName = CurrentSessionName;
			Info.HostUserId = UserId;
			Info.HostDisplayName = DisplayName;
		}

		Callback(Info);
	}

private:
	FEOSGamingService* Owner;

	EOS_HPlatform PlatformHandle = nullptr;
	EOS_HAuth AuthHandle = nullptr;
	EOS_HAchievements AchievementsHandle = nullptr;
	EOS_HLeaderboards LeaderboardsHandle = nullptr;
	EOS_HStats StatsHandle = nullptr;
	EOS_HConnect ConnectHandle = nullptr;
	EOS_HPlayerDataStorage PlayerDataStorageHandle = nullptr;
	EOS_HSessions SessionsHandle = nullptr;

	bool bIsInitialized = false;
	bool bIsConnected = false;
	bool bIsLoggedIn = false;
	bool bEOSSDKInitialized = false;
	FString UserId;
	FString DisplayName;
	EOS_EpicAccountId EpicAccountIdCached = nullptr;
	EOS_ProductUserId ProductUserId = nullptr;

	FString TempStoragePath;
	FString CurrentSessionName;
	bool bIsInSession = false;
	bool bIsSessionHost = false;

	TMap<FString, EOS_Achievements_DefinitionV2*> AchievementDefinitions;
	TMap<FString, EOS_Leaderboards_Definition*> LeaderboardDefinitions;
	bool bDefinitionsLoaded = false;

	void CreateUser(FAuthCallbackCtx* Ctx, EOS_ContinuanceToken ContinuanceToken) const
	{
		EOS_Connect_CreateUserOptions CreateOpts = {};
		CreateOpts.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
		CreateOpts.ContinuanceToken = ContinuanceToken;

		EOS_Connect_CreateUser(
			ConnectHandle,
			&CreateOpts,
			Ctx,
			[](const EOS_Connect_CreateUserCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAuthCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				if (!Service || !Service->Impl)
				{
					FAuthCallbackCtx::Complete(LocalCtx, FGamingServiceResult(false));
					return;
				}
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Connect create user failed: %d"),
					       (int32)Data->ResultCode);
					FAuthCallbackCtx::Complete(LocalCtx, FGamingServiceResult(false));
					return;
				}
				Service->Impl->CompleteAuthentication(Data->LocalUserId, LocalCtx);
			}
		);
	}

	bool InitializeEOSPlatform(const FEOSInitOptions& EOSOpts)
	{
		if (EOSOpts.ProductName.IsEmpty() || EOSOpts.ProductVersion.IsEmpty() ||
			EOSOpts.ProductId.IsEmpty() || EOSOpts.SandboxId.IsEmpty() ||
			EOSOpts.DeploymentId.IsEmpty() || EOSOpts.ClientId.IsEmpty() ||
			EOSOpts.ClientSecret.IsEmpty() || EOSOpts.EncryptionKey.IsEmpty())
		{
			UE_LOG(LogTemp, Error,
			       TEXT("EOSGamingService: EOS options incomplete. Provide all required fields in Initialize params."));
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "  Required: ProductName, ProductVersion, ProductId, SandboxId, DeploymentId, ClientId, ClientSecret, EncryptionKey"
			       ));
			if (EOSOpts.EncryptionKey.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("  Missing EncryptionKey! Generate one with: openssl rand -hex 32"));
			}
			return false;
		}

		EOS_InitializeOptions InitOptions = {};
		InitOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
		std::string ProductNameUtf8 = TCHAR_TO_UTF8(*EOSOpts.ProductName);
		std::string ProductVersionUtf8 = TCHAR_TO_UTF8(*EOSOpts.ProductVersion);
		InitOptions.ProductName = ProductNameUtf8.c_str();
		InitOptions.ProductVersion = ProductVersionUtf8.c_str();

		EOS_EResult InitResult = EOS_Initialize(&InitOptions);
		if (InitResult != EOS_EResult::EOS_Success && InitResult != EOS_EResult::EOS_AlreadyConfigured)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to initialize EOS SDK: %d"), (int32)InitResult);
			return false;
		}

		EOS_Logging_SetCallback(OnEOSLogMessage);
		EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Verbose);

		EOS_Platform_Options PlatformOptions = {};
		// For some reason Epic games made the latest version be 14 in this macro but the binaries say it only goes up to 13...
		//PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
		PlatformOptions.ApiVersion = 13;
		std::string ProductIdUtf8 = TCHAR_TO_UTF8(*EOSOpts.ProductId);
		std::string SandboxIdUtf8 = TCHAR_TO_UTF8(*EOSOpts.SandboxId);
		std::string DeploymentIdUtf8 = TCHAR_TO_UTF8(*EOSOpts.DeploymentId);
		std::string ClientIdUtf8 = TCHAR_TO_UTF8(*EOSOpts.ClientId);
		std::string ClientSecretUtf8 = TCHAR_TO_UTF8(*EOSOpts.ClientSecret);

		PlatformOptions.ProductId = ProductIdUtf8.empty() ? nullptr : ProductIdUtf8.c_str();
		PlatformOptions.SandboxId = SandboxIdUtf8.empty() ? nullptr : SandboxIdUtf8.c_str();
		PlatformOptions.DeploymentId = DeploymentIdUtf8.empty() ? nullptr : DeploymentIdUtf8.c_str();
		PlatformOptions.ClientCredentials.ClientId = ClientIdUtf8.empty() ? nullptr : ClientIdUtf8.c_str();
		PlatformOptions.ClientCredentials.ClientSecret = ClientSecretUtf8.empty() ? nullptr : ClientSecretUtf8.c_str();

		PlatformOptions.bIsServer = EOS_FALSE;
		PlatformOptions.OverrideCountryCode = nullptr;
		PlatformOptions.OverrideLocaleCode = nullptr;
		PlatformOptions.Flags = 0;
		PlatformOptions.TickBudgetInMilliseconds = 0;
		PlatformOptions.RTCOptions = nullptr;
		PlatformOptions.IntegratedPlatformOptionsContainerHandle = nullptr;
		PlatformOptions.SystemSpecificOptions = nullptr;
		PlatformOptions.TaskNetworkTimeoutSeconds = nullptr;

		std::string EncryptionKeyUtf8 = TCHAR_TO_UTF8(*EOSOpts.EncryptionKey);
		if (EncryptionKeyUtf8.length() != 64)
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "EOSGamingService: Invalid EncryptionKey length (%d). Must be exactly 64 hexadecimal characters."
			       ),
			       EncryptionKeyUtf8.length());
			UE_LOG(LogTemp, Error, TEXT("  Generate a valid key with: openssl rand -hex 32"));
			return false;
		}

		for (char c : EncryptionKeyUtf8)
		{
			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
			{
				UE_LOG(LogTemp, Error,
				       TEXT(
					       "EOSGamingService: Invalid EncryptionKey format. Must contain only hexadecimal characters (0-9, a-f, A-F)."
				       ));
				UE_LOG(LogTemp, Error, TEXT("  Generate a valid key with: openssl rand -hex 32"));
				return false;
			}
		}

		PlatformOptions.EncryptionKey = EncryptionKeyUtf8.c_str();
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Using encryption key for PlayerDataStorage"));

		FString CacheDir = FPaths::ProjectSavedDir() / TEXT("EOSCache");
		FPaths::MakeStandardFilename(CacheDir);
		FPaths::ConvertRelativePathToFull(CacheDir);
		IFileManager::Get().MakeDirectory(*CacheDir, true);
		PlatformOptions.CacheDirectory = TCHAR_TO_UTF8(*CacheDir);

		PlatformHandle = EOS_Platform_Create(&PlatformOptions);
		if (!PlatformHandle)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create EOS platform"));
			if (bEOSSDKInitialized)
			{
				EOS_Shutdown();
			}
			return false;
		}

		AuthHandle = EOS_Platform_GetAuthInterface(PlatformHandle);
		AchievementsHandle = EOS_Platform_GetAchievementsInterface(PlatformHandle);
		LeaderboardsHandle = EOS_Platform_GetLeaderboardsInterface(PlatformHandle);
		StatsHandle = EOS_Platform_GetStatsInterface(PlatformHandle);
		ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
		PlayerDataStorageHandle = EOS_Platform_GetPlayerDataStorageInterface(PlatformHandle);
		SessionsHandle = EOS_Platform_GetSessionsInterface(PlatformHandle);

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform created successfully"));
		return true;
	}

	void ShutdownEOSPlatform()
	{
		for (auto& Pair : AchievementDefinitions)
		{
			if (Pair.Value)
			{
				EOS_Achievements_DefinitionV2_Release(Pair.Value);
			}
		}
		AchievementDefinitions.Empty();

		for (auto& Pair : LeaderboardDefinitions)
		{
			if (Pair.Value)
			{
				EOS_Leaderboards_Definition_Release(Pair.Value);
			}
		}
		LeaderboardDefinitions.Empty();
		bDefinitionsLoaded = false;

		if (PlatformHandle)
		{
			EOS_Platform_Release(PlatformHandle);
			PlatformHandle = nullptr;
		}

		if (bEOSSDKInitialized)
		{
			EOS_Shutdown();
			bEOSSDKInitialized = false;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform shutdown"));
	}

	void ConvertEOSAchievementToGameAchievement(const EOS_Achievements_DefinitionV2* EOSDefinition,
	                                            const EOS_Achievements_PlayerAchievement* EOSPlayerAchievement,
	                                            FGameAchievement& GameAchievement)
	{
		if (EOSDefinition)
		{
			GameAchievement.Id = UTF8_TO_TCHAR(EOSDefinition->AchievementId);
			GameAchievement.DisplayName = UTF8_TO_TCHAR(EOSDefinition->UnlockedDisplayName);
			GameAchievement.Description = UTF8_TO_TCHAR(EOSDefinition->UnlockedDescription);
		}

		if (EOSPlayerAchievement)
		{
			GameAchievement.bIsUnlocked = (EOSPlayerAchievement->UnlockTime != 0);
			GameAchievement.Progress = EOSPlayerAchievement->Progress;
		}
	}

	void ConvertEOSLeaderboardRecordToEntry(const EOS_Leaderboards_LeaderboardRecord* EOSRecord,
	                                        FLeaderboardEntry& Entry)
	{
		if (EOSRecord)
		{
			Entry.UserId = UTF8_TO_TCHAR(EOSRecord->UserId);
			Entry.DisplayName = UTF8_TO_TCHAR(EOSRecord->UserDisplayName);
			Entry.Score = EOSRecord->Score;
			Entry.Rank = EOSRecord->Rank;
		}
	}

	static void EOS_CALL OnEOSLogMessage(const EOS_LogMessage* Message)
	{
		if (Message == nullptr)
		{
			return;
		}
		switch (Message->Level)
		{
		case EOS_ELogLevel::EOS_LOG_VeryVerbose:
		case EOS_ELogLevel::EOS_LOG_Verbose:
			UE_LOG(LogTemp, VeryVerbose, TEXT("[%hs] %hs"), Message->Category, Message->Message);
			break;
		case EOS_ELogLevel::EOS_LOG_Info:
			UE_LOG(LogTemp, Log, TEXT("[%hs] %hs"), Message->Category, Message->Message);
			break;
		case EOS_ELogLevel::EOS_LOG_Warning:
			UE_LOG(LogTemp, Warning, TEXT("[%hs] %hs"), Message->Category, Message->Message);
			break;
		case EOS_ELogLevel::EOS_LOG_Error:
		default:
			UE_LOG(LogTemp, Error, TEXT("[%hs] %hs"), Message->Category, Message->Message);
			break;
		}
	}

	void LoadAchievementDefinitions(TFunction<void(const bool&)> OnComplete)
	{
		checkf(AchievementsHandle,
		       TEXT("EOSGamingService: LoadAchievementDefinitions called when AchievementsHandle is not initialized"));

		auto* Ctx = FAchievementDefinitionsCallbackCtx::Create(Owner, MoveTemp(OnComplete));

		EOS_Achievements_QueryDefinitionsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_ACHIEVEMENTS_QUERYDEFINITIONS_API_LATEST;
		QueryOptions.LocalUserId = ProductUserId;

		EOS_Achievements_QueryDefinitions(
			AchievementsHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_Achievements_OnQueryDefinitionsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAchievementDefinitionsCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					Service->Impl->OnAchievementDefinitionsLoaded();
					FAchievementDefinitionsCallbackCtx::Complete(LocalCtx, true);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to query achievement definitions: %d"),
					       (int32)Data->ResultCode);
					FAchievementDefinitionsCallbackCtx::Complete(LocalCtx, false);
				}
			}
		);
	}

	void LoadLeaderboardDefinitions(TFunction<void(const bool&)> OnComplete)
	{
		checkf(LeaderboardsHandle,
		       TEXT("EOSGamingService: LoadLeaderboardDefinitions called when LeaderboardsHandle is not initialized"));

		auto* Ctx = FLeaderboardDefinitionsCallbackCtx::Create(Owner, MoveTemp(OnComplete));

		EOS_Leaderboards_QueryLeaderboardDefinitionsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDDEFINITIONS_API_LATEST;
		QueryOptions.LocalUserId = ProductUserId;

		EOS_Leaderboards_QueryLeaderboardDefinitions(
			LeaderboardsHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FLeaderboardDefinitionsCallbackCtx*>(Data->ClientData);
				FEOSGamingService* Service = LocalCtx ? LocalCtx->Service : nullptr;
				check(Service && Service->Impl);

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					Service->Impl->OnLeaderboardDefinitionsLoaded();
					FLeaderboardDefinitionsCallbackCtx::Complete(LocalCtx, true);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to query leaderboard definitions: %d"),
					       (int32)Data->ResultCode);
					FLeaderboardDefinitionsCallbackCtx::Complete(LocalCtx, false);
				}
			}
		);
	}

	void OnAchievementDefinitionsLoaded()
	{
		EOS_Achievements_GetAchievementDefinitionCountOptions CountOptions = {};
		CountOptions.ApiVersion = EOS_ACHIEVEMENTS_GETACHIEVEMENTDEFINITIONCOUNT_API_LATEST;
		uint32_t DefinitionCount = EOS_Achievements_GetAchievementDefinitionCount(AchievementsHandle, &CountOptions);

		for (uint32_t i = 0; i < DefinitionCount; ++i)
		{
			EOS_Achievements_CopyAchievementDefinitionV2ByIndexOptions CopyOptions = {};
			CopyOptions.ApiVersion = EOS_ACHIEVEMENTS_COPYACHIEVEMENTDEFINITIONV2BYINDEX_API_LATEST;
			CopyOptions.AchievementIndex = i;

			EOS_Achievements_DefinitionV2* Definition = nullptr;
			if (EOS_Achievements_CopyAchievementDefinitionV2ByIndex(AchievementsHandle, &CopyOptions, &Definition) ==
				EOS_EResult::EOS_Success && Definition)
			{
				FString AchievementId = UTF8_TO_TCHAR(Definition->AchievementId);
				AchievementDefinitions.Add(AchievementId, Definition);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cached achievement definition: %s"), *AchievementId);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Loaded %d achievement definitions"), DefinitionCount);
	}

	void OnLeaderboardDefinitionsLoaded()
	{
		EOS_Leaderboards_GetLeaderboardDefinitionCountOptions CountOptions = {};
		CountOptions.ApiVersion = EOS_LEADERBOARDS_GETLEADERBOARDDEFINITIONCOUNT_API_LATEST;
		uint32_t DefinitionCount = EOS_Leaderboards_GetLeaderboardDefinitionCount(LeaderboardsHandle, &CountOptions);

		for (uint32_t i = 0; i < DefinitionCount; ++i)
		{
			EOS_Leaderboards_CopyLeaderboardDefinitionByIndexOptions CopyOptions = {};
			CopyOptions.ApiVersion = EOS_LEADERBOARDS_COPYLEADERBOARDDEFINITIONBYINDEX_API_LATEST;
			CopyOptions.LeaderboardIndex = i;

			EOS_Leaderboards_Definition* Definition = nullptr;
			if (EOS_Leaderboards_CopyLeaderboardDefinitionByIndex(LeaderboardsHandle, &CopyOptions, &Definition) ==
				EOS_EResult::EOS_Success && Definition)
			{
				FString LeaderboardId = UTF8_TO_TCHAR(Definition->LeaderboardId);
				LeaderboardDefinitions.Add(LeaderboardId, Definition);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cached leaderboard definition: %s"), *LeaderboardId);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Loaded %d leaderboard definitions"), DefinitionCount);
	}

	void CompleteAuthentication(EOS_ProductUserId InProductUserId, FAuthCallbackCtx* AuthCtx)
	{
		bIsLoggedIn = true;
		ProductUserId = InProductUserId;

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Authentication successful, loading definitions..."));
		LoadAchievementDefinitions([this, AuthCtx](const bool& bSuccess)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Achievement definition loading failed"));
				FAuthCallbackCtx::Complete(AuthCtx, FGamingServiceResult(false));
				return;
			}

			LoadLeaderboardDefinitions([this, AuthCtx](const bool& bSuccess)
			{
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Leaderboard definition loading failed"));
					FAuthCallbackCtx::Complete(AuthCtx, FGamingServiceResult(false));
					return;
				}

				bDefinitionsLoaded = true;
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: All definitions loaded, starting cloud sync..."));

				SyncFromCloud([this, AuthCtx](const FGamingServiceResult& SyncResult)
				{
					if (!SyncResult.bSuccess)
					{
						UE_LOG(LogTemp, Warning,
						       TEXT("EOSGamingService: Cloud sync failed, but continuing with login"));
					}
					else
					{
						UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cloud sync completed successfully"));
					}

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Authentication complete"));
					FAuthCallbackCtx::Complete(AuthCtx, FGamingServiceResult(true));
				});
			});
		});
	}

public:
	FString GetFullLocalPath(const FString& RelativePath) const
	{
		if (TempStoragePath.IsEmpty())
		{
			return FPaths::ProjectSavedDir() / CloudStorageDirectoryName / RelativePath;
		}
		return TempStoragePath / RelativePath;
	}

	void SetTempStoragePath(const FString& InPath)
	{
		TempStoragePath = InPath;
		IFileManager::Get().MakeDirectory(*TempStoragePath, true);
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Set temp storage path to: %s"), *TempStoragePath);
	}

	const FString& GetTempStoragePath() const
	{
		return TempStoragePath;
	}

	struct FFileManifestEntry
	{
		int64 Timestamp;
		int64 Size;
	};

	struct FCloudManifest
	{
		TMap<FString, FFileManifestEntry> Files;
		int64 LastSyncTime = 0;

		FString ToJson() const
		{
			TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());

			TSharedPtr<FJsonObject> FilesObject = MakeShareable(new FJsonObject());
			for (const auto& Pair : Files)
			{
				TSharedPtr<FJsonObject> FileEntry = MakeShareable(new FJsonObject());
				FileEntry->SetNumberField(TEXT("timestamp"), Pair.Value.Timestamp);
				FileEntry->SetNumberField(TEXT("size"), Pair.Value.Size);
				FilesObject->SetObjectField(Pair.Key, FileEntry);
			}

			RootObject->SetObjectField(TEXT("files"), FilesObject);
			RootObject->SetNumberField(TEXT("last_sync_time"), LastSyncTime);

			FString OutputString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
			if (FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
			{
				return OutputString;
			}
			return TEXT("{}");
		}

		bool FromJson(const FString& JsonString)
		{
			Files.Empty();
			LastSyncTime = 0;

			TSharedPtr<FJsonObject> RootObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

			if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
			{
				return false;
			}

			if (RootObject->HasTypedField<EJson::Number>(TEXT("last_sync_time")))
			{
				LastSyncTime = static_cast<int64>(RootObject->GetNumberField(TEXT("last_sync_time")));
			}

			if (RootObject->HasTypedField<EJson::Object>(TEXT("files")))
			{
				TSharedPtr<FJsonObject> FilesObject = RootObject->GetObjectField(TEXT("files"));
				for (const auto& FilePair : FilesObject->Values)
				{
					if (FilePair.Value->Type == EJson::Object)
					{
						TSharedPtr<FJsonObject> FileEntry = FilePair.Value->AsObject();

						FFileManifestEntry Entry;
						Entry.Timestamp = static_cast<int64>(FileEntry->GetNumberField(TEXT("timestamp")));
						Entry.Size = static_cast<int64>(FileEntry->GetNumberField(TEXT("size")));

						Files.Add(FilePair.Key, Entry);
					}
				}
			}

			return true;
		}
	};

	FCloudManifest BuildLocalManifest()
	{
		FCloudManifest Manifest;
		FString BasePath = TempStoragePath.IsEmpty()
			                   ? (FPaths::ProjectSavedDir() / CloudStorageDirectoryName)
			                   : TempStoragePath;

		UE_LOG(LogTemp, VeryVerbose, TEXT("EOSGamingService: Building local manifest from: %s"), *BasePath);

		if (!FPaths::DirectoryExists(BasePath))
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Base directory does not exist: %s"), *BasePath);
			return Manifest;
		}

		TArray<FString> LocalFiles;
		IFileManager::Get().FindFilesRecursive(LocalFiles, *BasePath, TEXT("*"), true, false);
		UE_LOG(LogTemp, VeryVerbose, TEXT("EOSGamingService: Found %d files in directory"), LocalFiles.Num());

		FString BasePathWithSlash = BasePath;
		if (!BasePathWithSlash.EndsWith(TEXT("/")) && !BasePathWithSlash.EndsWith(TEXT("\\")))
		{
			BasePathWithSlash += TEXT("/");
		}

		for (const FString& FullPath : LocalFiles)
		{
			FString RelativePath = FullPath;
			if (RelativePath.StartsWith(BasePathWithSlash))
			{
				RelativePath = RelativePath.RightChop(BasePathWithSlash.Len());
			}
			else if (RelativePath.StartsWith(BasePath))
			{
				RelativePath = RelativePath.RightChop(BasePath.Len());
				if (RelativePath.StartsWith(TEXT("/")) || RelativePath.StartsWith(TEXT("\\")))
				{
					RelativePath = RelativePath.RightChop(1);
				}
			}

			if (RelativePath == ManifestFileName)
				continue;

			FFileManifestEntry Entry;
			Entry.Timestamp = IFileManager::Get().GetTimeStamp(*FullPath).ToUnixTimestamp();
			Entry.Size = IFileManager::Get().FileSize(*FullPath);

			if (Entry.Size > 0)
			{
				Manifest.Files.Add(RelativePath, Entry);
			}
		}

		Manifest.LastSyncTime = FDateTime::UtcNow().ToUnixTimestamp();
		return Manifest;
	}

	bool SaveLocalManifest(const FCloudManifest& Manifest)
	{
		FString BasePath = TempStoragePath.IsEmpty()
			                   ? (FPaths::ProjectSavedDir() / CloudStorageDirectoryName)
			                   : TempStoragePath;
		FString ManifestPath = BasePath / ManifestFileName;

		IFileManager::Get().MakeDirectory(*BasePath, true);
		return FFileHelper::SaveStringToFile(Manifest.ToJson(), *ManifestPath);
	}

	bool LoadLocalManifest(FCloudManifest& OutManifest)
	{
		FString BasePath = TempStoragePath.IsEmpty()
			                   ? (FPaths::ProjectSavedDir() / CloudStorageDirectoryName)
			                   : TempStoragePath;
		FString ManifestPath = BasePath / ManifestFileName;

		FString JsonContent;
		if (!FFileHelper::LoadFileToString(JsonContent, *ManifestPath))
		{
			return false;
		}

		return OutManifest.FromJson(JsonContent);
	}

	void DownloadFromCloudGeneric(const FString& FileName, TFunction<void(bool, const TArray<uint8>&)> Callback)
	{
		if (!bIsLoggedIn || !ProductUserId || !PlayerDataStorageHandle)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot download file %s - not logged in"), *FileName);
			Callback(false, TArray<uint8>());
			return;
		}

		struct FGenericDownloadCtx : FFileStorageCallbackCtx
		{
			FString FileName;
			TArray<uint8> FileData;
			TFunction<void(bool, const TArray<uint8>&)> DownloadCallback;
		};
		auto* Ctx = new FGenericDownloadCtx{};
		Ctx->Service = Owner;
		Ctx->FileName = FileName;
		Ctx->DownloadCallback = MoveTemp(Callback);

		EOS_PlayerDataStorage_ReadFileOptions ReadOptions = {};
		ReadOptions.ApiVersion = 1;
		ReadOptions.LocalUserId = ProductUserId;
		ReadOptions.Filename = TCHAR_TO_UTF8(*FileName);
		ReadOptions.ReadChunkLengthBytes = 1024 * 1024;
		ReadOptions.ReadFileDataCallback = [](
			const EOS_PlayerDataStorage_ReadFileDataCallbackInfo* Data) -> EOS_PlayerDataStorage_EReadResult
			{
				if (Data && Data->ClientData)
				{
					auto* LocalCtx = static_cast<FGenericDownloadCtx*>(Data->ClientData);
					if (Data->DataChunk && Data->DataChunkLengthBytes > 0)
					{
						LocalCtx->FileData.Append(static_cast<const uint8*>(Data->DataChunk),
						                          Data->DataChunkLengthBytes);
					}
				}
				return EOS_PlayerDataStorage_EReadResult::EOS_RR_ContinueReading;
			};
		ReadOptions.FileTransferProgressCallback = nullptr;

		EOS_PlayerDataStorage_ReadFile(
			PlayerDataStorageHandle,
			&ReadOptions,
			Ctx,
			[](const EOS_PlayerDataStorage_ReadFileCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FGenericDownloadCtx*>(Data->ClientData);

				bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!bSuccess && Data->ResultCode != EOS_EResult::EOS_NotFound)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to download %s: %d"), *LocalCtx->FileName,
					       (int32)Data->ResultCode);
				}

				LocalCtx->DownloadCallback(bSuccess, LocalCtx->FileData);
				delete LocalCtx;
			}
		);
	}

	void UploadToCloudGeneric(const FString& FileName, const TArray<uint8>& FileData, TFunction<void(bool)> Callback)
	{
		if (!bIsLoggedIn || !ProductUserId || !PlayerDataStorageHandle)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot upload file %s - not logged in"), *FileName);
			Callback(false);
			return;
		}

		struct FGenericUploadCtx : FFileStorageCallbackCtx
		{
			TArray<uint8> FileData;
			uint32_t CurrentOffset = 0;
			FString FileName;
			TFunction<void(bool)> UploadCallback;
		};
		auto* Ctx = new FGenericUploadCtx{};
		Ctx->Service = Owner;
		Ctx->FileData = FileData;
		Ctx->FileName = FileName;
		Ctx->UploadCallback = MoveTemp(Callback);

		EOS_PlayerDataStorage_WriteFileOptions WriteOptions = {};
		WriteOptions.ApiVersion = 1;
		WriteOptions.LocalUserId = ProductUserId;
		WriteOptions.Filename = TCHAR_TO_UTF8(*FileName);
		WriteOptions.ChunkLengthBytes = 4096;
		WriteOptions.WriteFileDataCallback = [](const EOS_PlayerDataStorage_WriteFileDataCallbackInfo* Data,
		                                        void* OutDataBuffer,
		                                        uint32_t* OutDataWritten) -> EOS_PlayerDataStorage_EWriteResult
		{
			if (Data && Data->ClientData && OutDataBuffer && OutDataWritten)
			{
				auto* LocalCtx = static_cast<FGenericUploadCtx*>(Data->ClientData);
				const uint32_t RemainingBytes = LocalCtx->FileData.Num() - LocalCtx->CurrentOffset;
				const uint32_t BytesToWrite = FMath::Min(Data->DataBufferLengthBytes, RemainingBytes);

				if (BytesToWrite > 0)
				{
					FMemory::Memcpy(OutDataBuffer, LocalCtx->FileData.GetData() + LocalCtx->CurrentOffset,
					                BytesToWrite);
					LocalCtx->CurrentOffset += BytesToWrite;
					*OutDataWritten = BytesToWrite;
					return EOS_PlayerDataStorage_EWriteResult::EOS_WR_ContinueWriting;
				}
			}
			*OutDataWritten = 0;
			return EOS_PlayerDataStorage_EWriteResult::EOS_WR_CompleteRequest;
		};
		WriteOptions.FileTransferProgressCallback = nullptr;

		EOS_PlayerDataStorage_WriteFile(
			PlayerDataStorageHandle,
			&WriteOptions,
			Ctx,
			[](const EOS_PlayerDataStorage_WriteFileCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FGenericUploadCtx*>(Data->ClientData);

				bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to upload %s: %d"), *LocalCtx->FileName,
					       (int32)Data->ResultCode);
				}

				LocalCtx->UploadCallback(bSuccess);
				delete LocalCtx;
			}
		);
	}

	void DownloadManifestFromCloud(TFunction<void(bool, const FCloudManifest&)> Callback)
	{
		DownloadFromCloudGeneric(ManifestFileName, [Callback](bool bSuccess, const TArray<uint8>& FileData)
		{
			FCloudManifest Manifest;

			if (bSuccess && FileData.Num() > 0)
			{
				FString JsonContent;
				FFileHelper::BufferToString(JsonContent, FileData.GetData(), FileData.Num());
				bSuccess = Manifest.FromJson(JsonContent);

				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Downloaded manifest from cloud with %d files"),
					       Manifest.Files.Num());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to parse manifest JSON"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: No manifest found in cloud (first sync)"));
				bSuccess = true;
				Manifest = FCloudManifest();
			}

			Callback(bSuccess, Manifest);
		});
	}

	void UploadManifestToCloud(const FCloudManifest& Manifest, TFunction<void(bool)> Callback)
	{
		FString JsonContent = Manifest.ToJson();
		TArray<uint8> FileData;
		FileData.Append((uint8*)TCHAR_TO_UTF8(*JsonContent), JsonContent.Len());

		UploadToCloudGeneric(ManifestFileName, FileData, [Callback](bool bSuccess)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Uploaded manifest to cloud"));
			}
			Callback(bSuccess);
		});
	}

	void DownloadFileFromCloud(const FString& FileName, TFunction<void(bool)> Callback)
	{
		DownloadFromCloudGeneric(FileName, [this, FileName, Callback](bool bSuccess, const TArray<uint8>& FileData)
		{
			if (bSuccess && FileData.Num() > 0)
			{
				FString FullPath = GetFullLocalPath(FileName);
				FString Directory = FPaths::GetPath(FullPath);
				IFileManager::Get().MakeDirectory(*Directory, true);

				if (FFileHelper::SaveArrayToFile(FileData, *FullPath))
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Downloaded file: %s"), *FileName);
					Callback(true);
					return;
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to save downloaded file: %s"), *FileName);
				}
			}

			Callback(false);
		});
	}

	void UploadFileToCloud(const FString& FileName, TFunction<void(bool)> Callback)
	{
		FString FullPath = GetFullLocalPath(FileName);

		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FullPath))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to read file for upload: %s"), *FileName);
			Callback(false);
			return;
		}

		UploadToCloudGeneric(FileName, FileData, [FileName, Callback](bool bSuccess)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Uploaded file: %s"), *FileName);
			}
			Callback(bSuccess);
		});
	}

	void DeleteFileFromCloud(const FString& FileName, TFunction<void(bool)> Callback)
	{
		if (!bIsLoggedIn || !ProductUserId || !PlayerDataStorageHandle)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot delete file %s - not logged in"), *FileName);
			Callback(false);
			return;
		}

		struct FDeleteCtx : FFileStorageCallbackCtx
		{
			FString FileName;
			TFunction<void(bool)> DeleteCallback;
		};
		auto* Ctx = new FDeleteCtx{};
		Ctx->Service = Owner;
		Ctx->FileName = FileName;
		Ctx->DeleteCallback = MoveTemp(Callback);

		EOS_PlayerDataStorage_DeleteFileOptions DeleteOptions = {};
		DeleteOptions.ApiVersion = 1;
		DeleteOptions.LocalUserId = ProductUserId;
		DeleteOptions.Filename = TCHAR_TO_UTF8(*FileName);

		EOS_PlayerDataStorage_DeleteFile(
			PlayerDataStorageHandle,
			&DeleteOptions,
			Ctx,
			[](const EOS_PlayerDataStorage_DeleteFileCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FDeleteCtx*>(Data->ClientData);

				bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Deleted file from cloud: %s"), *LocalCtx->FileName);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to delete file %s: %d"),
					       *LocalCtx->FileName, (int32)Data->ResultCode);
				}

				LocalCtx->DeleteCallback(bSuccess);
				delete LocalCtx;
			}
		);
	}

	void SyncFromCloud(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(bIsInitialized && bIsLoggedIn && PlayerDataStorageHandle,
		       TEXT("EOSGamingService: SyncFromCloud called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting sync from cloud..."));

		DownloadManifestFromCloud([this, Callback](bool bSuccess, const FCloudManifest& CloudManifest)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to download cloud manifest"));
				Callback(FGamingServiceResult(false));
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cloud manifest contains %d files"), CloudManifest.Files.Num());

			FCloudManifest LocalManifest = BuildLocalManifest();
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Local manifest contains %d files"), LocalManifest.Files.Num());

			TArray<FString> FilesToDownload;
			TArray<FString> FilesToDelete;
			for (const auto& CloudFilePair : CloudManifest.Files)
			{
				const FString& FileName = CloudFilePair.Key;
				const FFileManifestEntry& CloudEntry = CloudFilePair.Value;

				if (FileName == ManifestFileName)
				{
					continue;
				}
				
				if (!LocalManifest.Files.Contains(FileName))
				{
					FilesToDownload.Add(FileName);
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File missing locally, will download: %s"), *FileName);
				}
				else
				{
					const FFileManifestEntry& LocalEntry = LocalManifest.Files[FileName];

					if (CloudEntry.Timestamp > LocalEntry.Timestamp)
					{
						FilesToDownload.Add(FileName);
						UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cloud file is newer, will download: %s (cloud: %lld, local: %lld)"),
						       *FileName, CloudEntry.Timestamp, LocalEntry.Timestamp);
					}
				}
			}
			
			for (const auto& LocalFilePair : LocalManifest.Files)
			{
				const FString& FileName = LocalFilePair.Key;
				
				if (FileName == ManifestFileName)
				{
					continue;
				}
				
				if (!CloudManifest.Files.Contains(FileName))
				{
					FilesToDelete.Add(FileName);
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Local file not in cloud, will delete: %s"), *FileName);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Sync plan - %d files to download, %d files to delete"),
			       FilesToDownload.Num(), FilesToDelete.Num());

			ProcessSyncOperations(FilesToDownload, FilesToDelete, 0, 0, Callback);
		});
	}

	void ProcessSyncOperations(const TArray<FString>& FilesToDownload, const TArray<FString>& FilesToDelete,
	                           int32 DownloadIndex, int32 DeleteIndex,
	                           TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (DownloadIndex < FilesToDownload.Num())
		{
			const FString& FileName = FilesToDownload[DownloadIndex];
			DownloadFileFromCloud(FileName, [this, FilesToDownload, FilesToDelete, DownloadIndex, DeleteIndex, FileName, Callback](bool bSuccess)
			{
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to download file during sync: %s"), *FileName);
					Callback(FGamingServiceResult(false));
					return;
				}
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Downloaded file: %s"), *FileName);
				ProcessSyncOperations(FilesToDownload, FilesToDelete, DownloadIndex + 1, DeleteIndex, Callback);
			});
			return;
		}
		
		if (DeleteIndex < FilesToDelete.Num())
		{
			const FString& FileName = FilesToDelete[DeleteIndex];
			DeleteFile(FileName, [this, FilesToDownload, FilesToDelete, DownloadIndex, DeleteIndex, FileName, Callback](const FGamingServiceResult& Result)
			{
				if (!Result.bSuccess)
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to delete file during sync: %s"), *FileName);
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Deleted file: %s"), *FileName);
				}
				ProcessSyncOperations(FilesToDownload, FilesToDelete, DownloadIndex, DeleteIndex + 1, Callback);
			});
			return;
		}
		
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Regenerating local manifest..."));
		FCloudManifest UpdatedManifest = BuildLocalManifest();
		
		if (SaveLocalManifest(UpdatedManifest))
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Local manifest regenerated with %d files"), UpdatedManifest.Files.Num());
			Callback(FGamingServiceResult(true));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to save regenerated local manifest"));
			Callback(FGamingServiceResult(false));
		}
	}

	void SyncToCloud(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (!bIsInitialized || !bIsLoggedIn || !ProductUserId || !PlayerDataStorageHandle)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot sync to cloud - not logged in or service not ready"));
			Callback(FGamingServiceResult(false));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting shutdown sync to cloud..."));

		FCloudManifest OldLocalManifest;
		LoadLocalManifest(OldLocalManifest);

		FCloudManifest CurrentLocalManifest = BuildLocalManifest();

		TArray<FString> FilesToUpload;
		for (const auto& LocalFile : CurrentLocalManifest.Files)
		{
			const FString& FileName = LocalFile.Key;
			const FFileManifestEntry& CurrentEntry = LocalFile.Value;

			if (!OldLocalManifest.Files.Contains(FileName))
			{
				FilesToUpload.Add(FileName);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Will upload new file: %s"), *FileName);
			}
			else
			{
				const FFileManifestEntry& OldEntry = OldLocalManifest.Files[FileName];
				if (OldEntry.Timestamp != CurrentEntry.Timestamp || OldEntry.Size != CurrentEntry.Size)
				{
					FilesToUpload.Add(FileName);
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Will upload modified file: %s"), *FileName);
				}
			}
		}

		TArray<FString> FilesToDelete;
		for (const auto& OldFile : OldLocalManifest.Files)
		{
			if (!CurrentLocalManifest.Files.Contains(OldFile.Key))
			{
				FilesToDelete.Add(OldFile.Key);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Will delete removed file: %s"), *OldFile.Key);
			}
		}

		ExecuteShutdownSync(FilesToUpload, FilesToDelete, 0, 0, Callback);
	}

	void ExecuteShutdownSync(const TArray<FString>& FilesToUpload, const TArray<FString>& FilesToDelete,
	                         int32 UploadIndex, int32 DeleteIndex,
	                         TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (UploadIndex < FilesToUpload.Num())
		{
			const FString& FileName = FilesToUpload[UploadIndex];
			UploadFileToCloud(
				FileName,
				[this, FilesToUpload, FilesToDelete, UploadIndex, DeleteIndex, Callback](bool bSuccess)
				{
					if (!bSuccess)
					{
						UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Shutdown sync failed during upload"));
						Callback(FGamingServiceResult(false));
						return;
					}
					ExecuteShutdownSync(FilesToUpload, FilesToDelete, UploadIndex + 1, DeleteIndex,
					                    Callback);
				});
			return;
		}

		if (DeleteIndex < FilesToDelete.Num())
		{
			const FString& FileName = FilesToDelete[DeleteIndex];
			DeleteFileFromCloud(
				FileName,
				[this, FilesToUpload, FilesToDelete, UploadIndex, DeleteIndex, Callback](bool bSuccess)
				{
					if (!bSuccess)
					{
						UE_LOG(LogTemp, Warning,
						       TEXT("EOSGamingService: Failed to delete file during shutdown sync, continuing"));
					}
					ExecuteShutdownSync(FilesToUpload, FilesToDelete, UploadIndex, DeleteIndex + 1,
					                    Callback);
				});
			return;
		}
		
		FCloudManifest FinalManifest = BuildLocalManifest();
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Shutdown sync - saving manifest with %d files"), FinalManifest.Files.Num());
		SaveLocalManifest(FinalManifest);
		UploadManifestToCloud(FinalManifest, [Callback](bool bSuccess)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Shutdown sync completed successfully"));
				Callback(FGamingServiceResult(true));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to upload final manifest"));
				Callback(FGamingServiceResult(false));
			}
		});
	}
};

FEOSGamingService::FEOSGamingService()
{
	Impl = MakeUnique<FEOSGamingServiceImpl>(this);
}

FEOSGamingService::~FEOSGamingService()
{
}

bool FEOSGamingService::Connect(const FGamingServiceConnectParams& Params)
{
	return Impl->Connect(Params.EOS);
}

void FEOSGamingService::Shutdown()
{
	Impl->Shutdown();
}

void FEOSGamingService::UnlockAchievement(const FString& AchievementId,
                                          TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->UnlockAchievement(AchievementId, MoveTemp(Callback));
}

void FEOSGamingService::QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
{
	Impl->QueryAchievements(MoveTemp(Callback));
}

void FEOSGamingService::WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
                                              TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->WriteLeaderboardScore(LeaderboardId, Score, MoveTemp(Callback));
}

void FEOSGamingService::QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
                                             TFunction<void(const FLeaderboardResult&)> Callback)
{
	Impl->QueryLeaderboardPage(LeaderboardId, Limit, ContinuationToken, MoveTemp(Callback));
}

void FEOSGamingService::IngestStat(const FString& StatName, int32 Amount,
                                   TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->IngestStat(StatName, Amount, MoveTemp(Callback));
}

void FEOSGamingService::QueryStat(const FString& StatName,
                                  TFunction<void(const FStatQueryResult&)> Callback)
{
	Impl->QueryStat(StatName, MoveTemp(Callback));
}

void FEOSGamingService::WriteFile(const FString& FilePath, const TArray<uint8>& Data,
                                  TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->WriteFile(FilePath, Data, MoveTemp(Callback));
}

void FEOSGamingService::ReadFile(const FString& FilePath,
                                 TFunction<void(const FFileReadResult&)> Callback)
{
	Impl->ReadFile(FilePath, MoveTemp(Callback));
}

void FEOSGamingService::DeleteFile(const FString& FilePath,
                                   TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->DeleteFile(FilePath, MoveTemp(Callback));
}

void FEOSGamingService::ListFiles(const FString& DirectoryPath,
                                  TFunction<void(const FFilesListResult&)> Callback)
{
	Impl->ListFiles(DirectoryPath, MoveTemp(Callback));
}

void FEOSGamingService::Login(const FGamingServiceLoginParams& Params,
                              TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->AuthLogin(Params, MoveTemp(Callback));
}

void FEOSGamingService::Tick()
{
	Impl->Tick();
}

bool FEOSGamingService::IsInitialized() const
{
	return Impl->IsInitialized();
}

bool FEOSGamingService::IsLoggedIn() const
{
	return Impl->IsLoggedIn();
}

bool FEOSGamingService::NeedsLogin() const
{
	return Impl->NeedsLogin();
}

FString FEOSGamingService::GetUserId() const
{
	return Impl->GetUserId();
}

FString FEOSGamingService::GetDisplayName() const
{
	return Impl->GetDisplayName();
}

void FEOSGamingService::SetTempStoragePath(const FString& InPath)
{
	Impl->SetTempStoragePath(InPath);
}

const FString& FEOSGamingService::GetTempStoragePath() const
{
	return Impl->GetTempStoragePath();
}

void FEOSGamingService::CreateSession(const FSessionSettings& Settings,
                                      TFunction<void(const FSessionCreateResult&)> Callback)
{
	Impl->CreateSession(Settings, MoveTemp(Callback));
}

void FEOSGamingService::FindSessions(const FSessionSearchFilter& Filter,
                                     TFunction<void(const FSessionSearchResult&)> Callback)
{
	Impl->FindSessions(Filter, MoveTemp(Callback));
}

void FEOSGamingService::JoinSession(const FSessionJoinHandle& JoinHandle,
                                    TFunction<void(const FSessionJoinResult&)> Callback)
{
	Impl->JoinSession(JoinHandle, MoveTemp(Callback));
}

void FEOSGamingService::LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->LeaveSession(MoveTemp(Callback));
}

void FEOSGamingService::DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->DestroySession(MoveTemp(Callback));
}

void FEOSGamingService::UpdateSession(const FSessionSettings& Settings,
                                      TFunction<void(const FGamingServiceResult&)> Callback)
{
	Impl->UpdateSession(Settings, MoveTemp(Callback));
}

void FEOSGamingService::GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
{
	Impl->GetCurrentSession(MoveTemp(Callback));
}


#endif

#ifdef USE_EOS

#include "EOSGamingService.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

#include "eos_sdk.h"
#include "eos_common.h"
#include "eos_auth.h"
#include "eos_achievements.h"
#include "eos_stats.h"
#include "eos_leaderboards.h"
#include "eos_connect.h"
#include "eos_logging.h"

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

class FEOSGamingService::FEOSGamingServiceImpl
{
public:
	FEOSGamingServiceImpl(FEOSGamingService* InOwner)
		: Owner(InOwner)
	{
	}

	~FEOSGamingServiceImpl()
	{
		ShutdownEOSPlatform();
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

		bIsInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform initialized successfully"));
		return true;
	}

	void Shutdown()
	{
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting shutdown..."));

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

		// Query player achievements directly using cached definitions
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

				// Gather definitions into FGameAchievement array
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

		// Find the leaderboard definition by name
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

		// Get the stat name from the leaderboard definition
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
				Result.ContinuationToken = EndIndex < RecordCount ? EndIndex : -1; // -1 indicates no more entries
				FLeaderboardCallbackCtx::Complete(LocalCtx, Result);
			}
		);
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
		ConnLoginOpts.UserLoginInfo = nullptr; // Not required for EPIC credentials

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

private:
	FEOSGamingService* Owner;

	// EOS handles
	EOS_HPlatform PlatformHandle = nullptr;
	EOS_HAuth AuthHandle = nullptr;
	EOS_HAchievements AchievementsHandle = nullptr;
	EOS_HLeaderboards LeaderboardsHandle = nullptr;
	EOS_HStats StatsHandle = nullptr;
	EOS_HConnect ConnectHandle = nullptr;

	// Internal state
	bool bIsInitialized = false;
	bool bIsConnected = false;
	bool bIsLoggedIn = false;
	bool bEOSSDKInitialized = false;
	FString UserId;
	FString DisplayName;
	EOS_EpicAccountId EpicAccountIdCached = nullptr;
	EOS_ProductUserId ProductUserId = nullptr;

	// Cached definitions
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
		// Validate required fields were provided by caller
		if (EOSOpts.ProductName.IsEmpty() || EOSOpts.ProductVersion.IsEmpty() ||
			EOSOpts.ProductId.IsEmpty() || EOSOpts.SandboxId.IsEmpty() ||
			EOSOpts.DeploymentId.IsEmpty() || EOSOpts.ClientId.IsEmpty() ||
			EOSOpts.ClientSecret.IsEmpty())
		{
			UE_LOG(LogTemp, Error,
			       TEXT("EOSGamingService: EOS options incomplete. Provide all required fields in Initialize params."));
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

		// Configure EOS logging
		EOS_Logging_SetCallback(OnEOSLogMessage);
		EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Verbose);

		// Create platform
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

		// Set cache directory
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

		// Get interface handles
		AuthHandle = EOS_Platform_GetAuthInterface(PlatformHandle);
		AchievementsHandle = EOS_Platform_GetAchievementsInterface(PlatformHandle);
		LeaderboardsHandle = EOS_Platform_GetLeaderboardsInterface(PlatformHandle);
		StatsHandle = EOS_Platform_GetStatsInterface(PlatformHandle);
		ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform created successfully"));
		return true;
	}

	void ShutdownEOSPlatform()
	{
		// Clean up cached definitions
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

		// Release the platform handle
		if (PlatformHandle)
		{
			EOS_Platform_Release(PlatformHandle);
			PlatformHandle = nullptr;
		}

		// Shutdown the SDK if we initialized it
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
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: All definitions loaded, authentication complete"));
				FAuthCallbackCtx::Complete(AuthCtx, FGamingServiceResult(true));
			});
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


#endif

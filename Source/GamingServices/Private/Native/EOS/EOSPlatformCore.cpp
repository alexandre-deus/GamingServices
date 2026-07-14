#if defined(USE_EOS)

#include "Native/EOS/EOSPlatformCore.h"
#include "EOSCallbackContext.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"

#include <string>

namespace GamingServices
{
	// All EOS SDK handles / ids / cached definitions live here, hidden from the SDK-free core header.
	struct FEOSPlatformCore::FImpl
	{
		EOS_HPlatform PlatformHandle = nullptr;
		EOS_HAuth AuthHandle = nullptr;
		EOS_HAchievements AchievementsHandle = nullptr;
		EOS_HLeaderboards LeaderboardsHandle = nullptr;
		EOS_HStats StatsHandle = nullptr;
		EOS_HConnect ConnectHandle = nullptr;
		EOS_HPlayerDataStorage PlayerDataStorageHandle = nullptr;
		EOS_HLobby LobbyHandle = nullptr;
		EOS_HEcom EcomHandle = nullptr;
		EOS_HUserInfo UserInfoHandle = nullptr;

		EOS_EpicAccountId EpicAccountIdCached = nullptr;
		EOS_ProductUserId ProductUserId = nullptr;

		TMap<FString, EOS_Achievements_DefinitionV2*> AchievementDefinitions;
		TMap<FString, EOS_Leaderboards_Definition*> LeaderboardDefinitions;
	};

	// Login is an internal multi-step flow (Auth -> Connect -> [CreateUser] -> definitions -> cloud sync).
	// Its callbacks need to reach the core, so the context's Service is the core itself.
	struct FEOSCorePlatformLoginCtx : public TEOSCallbackContext<FGamingServiceResult, FEOSPlatformCore>
	{
	};

	using FCoreLoginCtx = FEOSCorePlatformLoginCtx;
	using FCoreBoolCtx = TEOSCallbackContext<bool, FEOSPlatformCore>;

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

	FEOSPlatformCore::FEOSPlatformCore()
	{
		Impl = MakePimpl<FImpl>();
	}

	FEOSPlatformCore::~FEOSPlatformCore() = default;

	void* FEOSPlatformCore::GetPlatformHandle() const { return Impl->PlatformHandle; }
	void* FEOSPlatformCore::GetAchievementsHandle() const { return Impl->AchievementsHandle; }
	void* FEOSPlatformCore::GetLeaderboardsHandle() const { return Impl->LeaderboardsHandle; }
	void* FEOSPlatformCore::GetStatsHandle() const { return Impl->StatsHandle; }
	void* FEOSPlatformCore::GetPlayerDataStorageHandle() const { return Impl->PlayerDataStorageHandle; }
	void* FEOSPlatformCore::GetLobbyHandle() const { return Impl->LobbyHandle; }
	void* FEOSPlatformCore::GetConnectHandle() const { return Impl->ConnectHandle; }
	void* FEOSPlatformCore::GetUserInfoHandle() const { return Impl->UserInfoHandle; }
	void* FEOSPlatformCore::GetEcomHandle() const { return Impl->EcomHandle; }

	void* FEOSPlatformCore::GetEpicAccountId() const { return Impl->EpicAccountIdCached; }
	void* FEOSPlatformCore::GetProductUserId() const { return Impl->ProductUserId; }

	TArray<const void*> FEOSPlatformCore::GetAchievementDefinitionPtrs() const
	{
		TArray<const void*> Out;
		Out.Reserve(Impl->AchievementDefinitions.Num());
		for (const auto& Pair : Impl->AchievementDefinitions)
		{
			Out.Add(Pair.Value);
		}
		return Out;
	}

	const void* FEOSPlatformCore::FindLeaderboardDefinition(const FString& LeaderboardId) const
	{
		if (const EOS_Leaderboards_Definition* const* Found = Impl->LeaderboardDefinitions.Find(LeaderboardId))
		{
			return *Found;
		}
		return nullptr;
	}

	void FEOSPlatformCore::InitializePlatform(const FEOSInitOptions& Overrides)
	{
		const TCHAR* Section = TEXT("GamingServices.EOS");
		FEOSInitOptions Opts;

		// Config supplies the defaults; any non-empty field in Overrides wins. Per-field overrides let
		// a test harness run several local instances against different EOS clients (e.g. one Dev Auth
		// Tool client per instance) without touching the ini.
		const struct { const TCHAR* Key; const FString& Override; FString& Value; } Fields[] = {
			{ TEXT("ProductName"),    Overrides.ProductName,    Opts.ProductName },
			{ TEXT("ProductVersion"), Overrides.ProductVersion, Opts.ProductVersion },
			{ TEXT("ProductId"),      Overrides.ProductId,      Opts.ProductId },
			{ TEXT("SandboxId"),      Overrides.SandboxId,      Opts.SandboxId },
			{ TEXT("DeploymentId"),   Overrides.DeploymentId,   Opts.DeploymentId },
			{ TEXT("ClientId"),       Overrides.ClientId,       Opts.ClientId },
			{ TEXT("ClientSecret"),   Overrides.ClientSecret,   Opts.ClientSecret },
			{ TEXT("EncryptionKey"),  Overrides.EncryptionKey,  Opts.EncryptionKey },
		};

		TArray<FString> MissingKeys;
		for (const auto& Field : Fields)
		{
			GConfig->GetString(Section, Field.Key, Field.Value, GGameIni);
			if (!Field.Override.IsEmpty())
			{
				Field.Value = Field.Override;
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: %s overridden by init params"), Field.Key);
			}
			if (Field.Value.IsEmpty())
			{
				MissingKeys.Add(Field.Key);
			}
		}

		if (MissingKeys.Num() > 0)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Missing required EOS settings: %s. "
				"Each must come from [GamingServices.EOS] in Game.ini or a non-empty FEOSInitOptions override."),
				*FString::Join(MissingKeys, TEXT(", ")));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting initialization..."));

		if (!InitializeEOSPlatform(Opts))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to initialize EOS platform"));
			return;
		}

		if (!Impl->PlatformHandle)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Initialization failed"));
			return;
		}

		if (TempStoragePath.IsEmpty())
		{
			TempStoragePath = FPaths::ProjectSavedDir() / CloudStorageDirectoryName;
			IFileManager::Get().MakeDirectory(*TempStoragePath, true);
		}

		bIsInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform initialized successfully"));
	}

	void FEOSPlatformCore::DestroyPlatform()
	{
		Shutdown();
		ShutdownEOSPlatform();
	}

	void FEOSPlatformCore::Shutdown()
	{
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting shutdown..."));

		if (bIsLoggedIn && Impl->ProductUserId && Impl->PlayerDataStorageHandle && SyncToCloudHook)
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Syncing to cloud before shutdown..."));

			bool bSyncCompleted = false;

			SyncToCloudHook([&bSyncCompleted](const FGamingServiceResult& Result)
			{
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
				if (Impl->PlatformHandle)
				{
					EOS_Platform_Tick(Impl->PlatformHandle);
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

		// Unregister the matchmaking notifications (owned by FEOSMatchmaking, torn down here so the ordering
		// matches the legacy flow where they were removed during core shutdown).
		if (UnregisterMatchmakingNotificationsHook)
		{
			UnregisterMatchmakingNotificationsHook();
		}

		bIsConnected = false;
		bIsLoggedIn = false;
		UserId.Empty();
		DisplayName.Empty();

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Shutdown completed"));
	}

	void FEOSPlatformCore::Tick()
	{
		if (Impl->PlatformHandle)
		{
			EOS_Platform_Tick(Impl->PlatformHandle);
		}
	}

	FString FEOSPlatformCore::GetFullLocalPath(const FString& RelativePath) const
	{
		if (TempStoragePath.IsEmpty())
		{
			return FPaths::ProjectSavedDir() / CloudStorageDirectoryName / RelativePath;
		}
		return TempStoragePath / RelativePath;
	}

	void FEOSPlatformCore::SetTempStoragePath(const FString& InPath)
	{
		TempStoragePath = InPath;
		IFileManager::Get().MakeDirectory(*TempStoragePath, true);
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Set temp storage path to: %s"), *TempStoragePath);
	}

	void FEOSPlatformCore::Login(const FGamingServiceLoginParams& Params,
	                             TFunction<void(const FGamingServiceResult&)> Callback)
	{
		AuthLogin(Params, MoveTemp(Callback));
	}

	void FEOSPlatformCore::AuthLogin(const FGamingServiceLoginParams& Params,
	                                 TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Impl->AuthHandle, TEXT("EOSGamingService: AuthLogin called when AuthHandle is not initialized"));

		EOS_Auth_LoginOptions LoginOptions = {};
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_BasicProfile;

		EOS_Auth_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

		// These must stay alive until EOS_Auth_Login below has copied the credential strings;
		// assigning TCHAR_TO_UTF8() directly to Credentials leaves dangling pointers.
		const std::string DevHostUtf8 = TCHAR_TO_UTF8(*Params.EOS.DeveloperHost);
		const std::string DevCredUtf8 = TCHAR_TO_UTF8(*Params.EOS.DeveloperCredentialName);

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
			if (!DevHostUtf8.empty())
			{
				Credentials.Id = DevHostUtf8.c_str();
			}
			if (!DevCredUtf8.empty())
			{
				Credentials.Token = DevCredUtf8.c_str();
			}
			break;
		default:
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
			break;
		}

		LoginOptions.Credentials = &Credentials;

		auto* Ctx = new FCoreLoginCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);

		EOS_Auth_Login(
			Impl->AuthHandle,
			&LoginOptions,
			Ctx,
			[](const EOS_Auth_LoginCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FCoreLoginCtx*>(Data->ClientData);
				FEOSPlatformCore* Core = LocalCtx ? LocalCtx->Service : nullptr;
				checkf(Core,
				       TEXT("EOSGamingService: Auth login failed because Core is not initialized"));
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Auth login failed: %d"), (int32)Data->ResultCode);
					FCoreLoginCtx::Complete(LocalCtx, FGamingServiceResult(false));
					return;
				}

				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Auth login successful"));
				Core->Impl->EpicAccountIdCached = Data->LocalUserId;
				Core->ConnectLogin(Data->LocalUserId, [LocalCtx](const FGamingServiceResult& ConnectResult)
				{
					FCoreLoginCtx::Complete(LocalCtx, ConnectResult);
				});
			}
		);
	}

	void FEOSPlatformCore::ConnectLogin(void* EpicAccountId,
	                                    TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Impl->AuthHandle && Impl->ConnectHandle,
		       TEXT("EOSGamingService: ConnectLogin called when handles are not initialized"));

		EOS_EpicAccountId LocalEpicAccountId = static_cast<EOS_EpicAccountId>(EpicAccountId);

		EOS_Auth_Token* AuthToken = nullptr;
		EOS_Auth_CopyUserAuthTokenOptions CopyOpts = {};
		CopyOpts.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;
		EOS_EResult CopyRes = EOS_Auth_CopyUserAuthToken(Impl->AuthHandle, &CopyOpts, LocalEpicAccountId, &AuthToken);
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

		auto* Ctx = new FCoreLoginCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);

		EOS_Connect_Login(
			Impl->ConnectHandle,
			&ConnLoginOpts,
			Ctx,
			[](const EOS_Connect_LoginCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FCoreLoginCtx*>(Data->ClientData);
				FEOSPlatformCore* Core = LocalCtx ? LocalCtx->Service : nullptr;
				checkf(Core,
				       TEXT("EOSGamingService: Connect login failed because Core is not initialized"));

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Connect login successful"));
					Core->CompleteAuthentication(Data->LocalUserId, LocalCtx);
					return;
				}

				if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && Data->ContinuanceToken)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: User not found, creating new user"));
					Core->CreateUser(LocalCtx, Data->ContinuanceToken);
					return;
				}

				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Connect login failed: %d"), (int32)Data->ResultCode);
				FCoreLoginCtx::Complete(LocalCtx, FGamingServiceResult(false));
			}
		);
	}

	void FEOSPlatformCore::CreateUser(FEOSCorePlatformLoginCtx* Ctx, void* ContinuanceToken) const
	{
		EOS_Connect_CreateUserOptions CreateOpts = {};
		CreateOpts.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
		CreateOpts.ContinuanceToken = static_cast<EOS_ContinuanceToken>(ContinuanceToken);

		EOS_Connect_CreateUser(
			Impl->ConnectHandle,
			&CreateOpts,
			Ctx,
			[](const EOS_Connect_CreateUserCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FCoreLoginCtx*>(Data->ClientData);
				FEOSPlatformCore* Core = LocalCtx ? LocalCtx->Service : nullptr;
				if (!Core)
				{
					FCoreLoginCtx::Complete(LocalCtx, FGamingServiceResult(false));
					return;
				}
				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Connect create user failed: %d"),
					       (int32)Data->ResultCode);
					FCoreLoginCtx::Complete(LocalCtx, FGamingServiceResult(false));
					return;
				}
				Core->CompleteAuthentication(Data->LocalUserId, LocalCtx);
			}
		);
	}

	void FEOSPlatformCore::CompleteAuthentication(void* InProductUserId, FEOSCorePlatformLoginCtx* AuthCtx)
	{
		bIsLoggedIn = true;
		Impl->ProductUserId = static_cast<EOS_ProductUserId>(InProductUserId);

		// Cache the identity fields served by IUserService. UserId is the ProductUserId (the id the
		// stats / leaderboards / sessions APIs key on); DisplayName is fetched asynchronously below.
		char PuidStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
		int32_t PuidLen = sizeof(PuidStr);
		if (EOS_ProductUserId_ToString(Impl->ProductUserId, PuidStr, &PuidLen) == EOS_EResult::EOS_Success)
		{
			UserId = UTF8_TO_TCHAR(PuidStr);
		}

		// Register the matchmaking notifications (lobby invite-accepted + member-status). They conceptually
		// belong to FEOSMatchmaking, but must be registered at this point in the login flow (right after
		// ProductUserId is set and the lobby handle is valid), so the core fires the matchmaking-owned hook
		// rather than doing the registration itself.
		if (RegisterMatchmakingNotificationsHook)
		{
			RegisterMatchmakingNotificationsHook();
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Authentication successful, resolving display name..."));
		QueryDisplayName([this, AuthCtx]()
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Loading definitions..."));
			LoadAchievementDefinitions([this, AuthCtx](const bool& bSuccess)
			{
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Achievement definition loading failed"));
					FCoreLoginCtx::Complete(AuthCtx, FGamingServiceResult(false));
					return;
				}

				LoadLeaderboardDefinitions([this, AuthCtx](const bool& bSuccess)
				{
					if (!bSuccess)
					{
						UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Leaderboard definition loading failed"));
						FCoreLoginCtx::Complete(AuthCtx, FGamingServiceResult(false));
						return;
					}

					bDefinitionsLoaded = true;
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: All definitions loaded, starting cloud sync..."));

					if (!SyncFromCloudHook)
					{
						UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: No cloud sync hook bound, completing login"));
						UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Authentication complete"));
						FCoreLoginCtx::Complete(AuthCtx, FGamingServiceResult(true));
						return;
					}

					SyncFromCloudHook([this, AuthCtx](const FGamingServiceResult& SyncResult)
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
						FCoreLoginCtx::Complete(AuthCtx, FGamingServiceResult(true));
					});
				});
			});
		});
	}

	void FEOSPlatformCore::QueryDisplayName(TFunction<void()> OnComplete)
	{
		// Best-effort: a missing display name must never fail the login, so every path continues.
		if (!Impl->UserInfoHandle || !Impl->EpicAccountIdCached)
		{
			OnComplete();
			return;
		}

		struct FDisplayNameCtx
		{
			FEOSPlatformCore* Core;
			TFunction<void()> OnComplete;
		};
		auto* Ctx = new FDisplayNameCtx{this, MoveTemp(OnComplete)};

		EOS_UserInfo_QueryUserInfoOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
		QueryOptions.LocalUserId = Impl->EpicAccountIdCached;
		QueryOptions.TargetUserId = Impl->EpicAccountIdCached;

		EOS_UserInfo_QueryUserInfo(
			Impl->UserInfoHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_UserInfo_QueryUserInfoCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FDisplayNameCtx> LocalCtx(static_cast<FDisplayNameCtx*>(Data->ClientData));
				FEOSPlatformCore* Core = LocalCtx->Core;

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					const auto ExtractName = [Core](EOS_UserInfo_BestDisplayName* Best)
					{
						if (Best)
						{
							const char* Name = Best->DisplayName ? Best->DisplayName : Best->DisplayNameSanitized;
							if (Name)
							{
								Core->DisplayName = UTF8_TO_TCHAR(Name);
							}
							EOS_UserInfo_BestDisplayName_Release(Best);
						}
					};

					EOS_UserInfo_CopyBestDisplayNameOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_USERINFO_COPYBESTDISPLAYNAME_API_LATEST;
					CopyOptions.LocalUserId = Core->Impl->EpicAccountIdCached;
					CopyOptions.TargetUserId = Core->Impl->EpicAccountIdCached;

					EOS_UserInfo_BestDisplayName* Best = nullptr;
					if (EOS_UserInfo_CopyBestDisplayName(Core->Impl->UserInfoHandle, &CopyOptions, &Best) ==
						EOS_EResult::EOS_Success)
					{
						ExtractName(Best);
					}
					else
					{
						// Accounts without a linked external platform (e.g. Dev Auth Tool logins) report
						// BestDisplayNameIndeterminate; per SDK docs, fall back to the Epic platform name.
						EOS_UserInfo_CopyBestDisplayNameWithPlatformOptions PlatformOptions = {};
						PlatformOptions.ApiVersion = EOS_USERINFO_COPYBESTDISPLAYNAMEWITHPLATFORM_API_LATEST;
						PlatformOptions.LocalUserId = Core->Impl->EpicAccountIdCached;
						PlatformOptions.TargetUserId = Core->Impl->EpicAccountIdCached;
						PlatformOptions.TargetPlatformType = EOS_OPT_Epic;

						EOS_UserInfo_BestDisplayName* PlatformBest = nullptr;
						if (EOS_UserInfo_CopyBestDisplayNameWithPlatform(
							Core->Impl->UserInfoHandle, &PlatformOptions, &PlatformBest) == EOS_EResult::EOS_Success)
						{
							ExtractName(PlatformBest);
						}
					}
				}

				if (Core->DisplayName.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Could not resolve display name (result %d)"),
					       (int32)Data->ResultCode);
				}
				LocalCtx->OnComplete();
			});
	}

	bool FEOSPlatformCore::InitializeEOSPlatform(const FEOSInitOptions& EOSOpts)
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
		// Must outlive the EOS_Platform_Create call below; assigning TCHAR_TO_UTF8() directly leaves
		// a dangling pointer.
		const std::string CacheDirUtf8 = TCHAR_TO_UTF8(*CacheDir);
		PlatformOptions.CacheDirectory = CacheDirUtf8.c_str();

		Impl->PlatformHandle = EOS_Platform_Create(&PlatformOptions);
		if (!Impl->PlatformHandle)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create EOS platform"));
			if (bEOSSDKInitialized)
			{
				EOS_Shutdown();
			}
			return false;
		}

		Impl->AuthHandle = EOS_Platform_GetAuthInterface(Impl->PlatformHandle);
		Impl->AchievementsHandle = EOS_Platform_GetAchievementsInterface(Impl->PlatformHandle);
		Impl->LeaderboardsHandle = EOS_Platform_GetLeaderboardsInterface(Impl->PlatformHandle);
		Impl->StatsHandle = EOS_Platform_GetStatsInterface(Impl->PlatformHandle);
		Impl->ConnectHandle = EOS_Platform_GetConnectInterface(Impl->PlatformHandle);
		Impl->PlayerDataStorageHandle = EOS_Platform_GetPlayerDataStorageInterface(Impl->PlatformHandle);
		Impl->LobbyHandle = EOS_Platform_GetLobbyInterface(Impl->PlatformHandle);
		Impl->EcomHandle = EOS_Platform_GetEcomInterface(Impl->PlatformHandle);
		Impl->UserInfoHandle = EOS_Platform_GetUserInfoInterface(Impl->PlatformHandle);

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform created successfully"));
		return true;
	}

	void FEOSPlatformCore::ShutdownEOSPlatform()
	{
		for (auto& Pair : Impl->AchievementDefinitions)
		{
			if (Pair.Value)
			{
				EOS_Achievements_DefinitionV2_Release(Pair.Value);
			}
		}
		Impl->AchievementDefinitions.Empty();

		for (auto& Pair : Impl->LeaderboardDefinitions)
		{
			if (Pair.Value)
			{
				EOS_Leaderboards_Definition_Release(Pair.Value);
			}
		}
		Impl->LeaderboardDefinitions.Empty();
		bDefinitionsLoaded = false;

		if (Impl->PlatformHandle)
		{
			EOS_Platform_Release(Impl->PlatformHandle);
			Impl->PlatformHandle = nullptr;
		}

		if (bEOSSDKInitialized)
		{
			EOS_Shutdown();
			bEOSSDKInitialized = false;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS platform shutdown"));
	}

	void FEOSPlatformCore::LoadAchievementDefinitions(TFunction<void(const bool&)> OnComplete)
	{
		checkf(Impl->AchievementsHandle,
		       TEXT("EOSGamingService: LoadAchievementDefinitions called when AchievementsHandle is not initialized"));

		auto* Ctx = FCoreBoolCtx::Create(this, MoveTemp(OnComplete));

		EOS_Achievements_QueryDefinitionsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_ACHIEVEMENTS_QUERYDEFINITIONS_API_LATEST;
		QueryOptions.LocalUserId = Impl->ProductUserId;

		EOS_Achievements_QueryDefinitions(
			Impl->AchievementsHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_Achievements_OnQueryDefinitionsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FCoreBoolCtx*>(Data->ClientData);
				FEOSPlatformCore* Core = LocalCtx ? LocalCtx->Service : nullptr;
				check(Core);

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					Core->OnAchievementDefinitionsLoaded();
					FCoreBoolCtx::Complete(LocalCtx, true);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to query achievement definitions: %d"),
					       (int32)Data->ResultCode);
					FCoreBoolCtx::Complete(LocalCtx, false);
				}
			}
		);
	}

	void FEOSPlatformCore::LoadLeaderboardDefinitions(TFunction<void(const bool&)> OnComplete)
	{
		checkf(Impl->LeaderboardsHandle,
		       TEXT("EOSGamingService: LoadLeaderboardDefinitions called when LeaderboardsHandle is not initialized"));

		auto* Ctx = FCoreBoolCtx::Create(this, MoveTemp(OnComplete));

		EOS_Leaderboards_QueryLeaderboardDefinitionsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDDEFINITIONS_API_LATEST;
		QueryOptions.LocalUserId = Impl->ProductUserId;

		EOS_Leaderboards_QueryLeaderboardDefinitions(
			Impl->LeaderboardsHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FCoreBoolCtx*>(Data->ClientData);
				FEOSPlatformCore* Core = LocalCtx ? LocalCtx->Service : nullptr;
				check(Core);

				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					Core->OnLeaderboardDefinitionsLoaded();
					FCoreBoolCtx::Complete(LocalCtx, true);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to query leaderboard definitions: %d"),
					       (int32)Data->ResultCode);
					FCoreBoolCtx::Complete(LocalCtx, false);
				}
			}
		);
	}

	void FEOSPlatformCore::OnAchievementDefinitionsLoaded()
	{
		EOS_Achievements_GetAchievementDefinitionCountOptions CountOptions = {};
		CountOptions.ApiVersion = EOS_ACHIEVEMENTS_GETACHIEVEMENTDEFINITIONCOUNT_API_LATEST;
		uint32_t DefinitionCount = EOS_Achievements_GetAchievementDefinitionCount(Impl->AchievementsHandle, &CountOptions);

		for (uint32_t i = 0; i < DefinitionCount; ++i)
		{
			EOS_Achievements_CopyAchievementDefinitionV2ByIndexOptions CopyOptions = {};
			CopyOptions.ApiVersion = EOS_ACHIEVEMENTS_COPYACHIEVEMENTDEFINITIONV2BYINDEX_API_LATEST;
			CopyOptions.AchievementIndex = i;

			EOS_Achievements_DefinitionV2* Definition = nullptr;
			if (EOS_Achievements_CopyAchievementDefinitionV2ByIndex(Impl->AchievementsHandle, &CopyOptions, &Definition) ==
				EOS_EResult::EOS_Success && Definition)
			{
				FString AchievementId = UTF8_TO_TCHAR(Definition->AchievementId);
				Impl->AchievementDefinitions.Add(AchievementId, Definition);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cached achievement definition: %s"), *AchievementId);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Loaded %d achievement definitions"), DefinitionCount);
	}

	void FEOSPlatformCore::OnLeaderboardDefinitionsLoaded()
	{
		EOS_Leaderboards_GetLeaderboardDefinitionCountOptions CountOptions = {};
		CountOptions.ApiVersion = EOS_LEADERBOARDS_GETLEADERBOARDDEFINITIONCOUNT_API_LATEST;
		uint32_t DefinitionCount = EOS_Leaderboards_GetLeaderboardDefinitionCount(Impl->LeaderboardsHandle, &CountOptions);

		for (uint32_t i = 0; i < DefinitionCount; ++i)
		{
			EOS_Leaderboards_CopyLeaderboardDefinitionByIndexOptions CopyOptions = {};
			CopyOptions.ApiVersion = EOS_LEADERBOARDS_COPYLEADERBOARDDEFINITIONBYINDEX_API_LATEST;
			CopyOptions.LeaderboardIndex = i;

			EOS_Leaderboards_Definition* Definition = nullptr;
			if (EOS_Leaderboards_CopyLeaderboardDefinitionByIndex(Impl->LeaderboardsHandle, &CopyOptions, &Definition) ==
				EOS_EResult::EOS_Success && Definition)
			{
				FString LeaderboardId = UTF8_TO_TCHAR(Definition->LeaderboardId);
				Impl->LeaderboardDefinitions.Add(LeaderboardId, Definition);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cached leaderboard definition: %s"), *LeaderboardId);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Loaded %d leaderboard definitions"), DefinitionCount);
	}
}

#endif // USE_EOS

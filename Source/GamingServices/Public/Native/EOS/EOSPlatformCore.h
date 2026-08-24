#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "DataTypes/ConnectTypes.h"
#include "DataTypes/GamingServiceResult.h"
#include "DataTypes/LoginTypes.h"
#include "Native/Interfaces/IExternalAuthService.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	/**
	 * Owns the EOS platform handle and every sub-interface handle, plus shared identity / login state.
	 * The per-capability EOS classes hold a reference to this core and reach the handles + state they
	 * need through it.
	 *
	 * This header is SDK-free: every EOS handle / id is stored in a private FImpl (defined in the .cpp)
	 * and exposed only through opaque void* accessors that the capability .cpp files cast back to the
	 * concrete EOS_* type. No eos_*.h is included here.
	 *
	 * This is the EOS equivalent of the legacy FEOSGamingServiceImpl pimpl, minus the per-capability
	 * gameplay calls (which now live in the FEOS* capability classes).
	 */
	class FEOSPlatformCore
	{
	public:
		static constexpr const TCHAR* CloudStorageDirectoryName = TEXT("EOSRemoteStorage");
		static constexpr const TCHAR* ManifestFileName = TEXT("manifest.json");

		FEOSPlatformCore();
		~FEOSPlatformCore();

		// Platform lifecycle. Any non-empty field in Overrides wins over its [GamingServices.EOS] config
		// value, so a test harness can point each local instance at a different EOS client.
		void InitializePlatform(const FEOSInitOptions& Overrides = FEOSInitOptions());
		void DestroyPlatform();
		void Tick();

		// Login / identity (Auth + Connect flow) against an Epic account.
		void Login(const FGamingServiceLoginParams& Params, TFunction<void(const FGamingServiceResult&)> Callback);

		/**
		 * Login using a credential minted by a different platform (e.g. a Steam session ticket), going
		 * straight to EOS Connect and skipping the Epic Auth interface entirely.
		 *
		 * There is no EpicAccountId in this flow — the ProductUserId is the identity, which is all the
		 * feature services (lobbies, P2P, stats, achievements, player data storage) key on. The account
		 * is created on first sight. DisplayName is passed through as non-authoritative user info so the
		 * originating platform's name still shows up on leaderboards and in lobbies.
		 *
		 * On success the core is logged in exactly as it would be after Login(), so every EOS capability
		 * behaves identically regardless of which platform did the authenticating.
		 */
		void LoginWithExternalCredential(EExternalCredentialType CredentialType, const FString& Token,
		                                 const FString& InDisplayName,
		                                 TFunction<void(const FGamingServiceResult&)> Callback);

		/** Whether this core can consume the given credential format. */
		static bool SupportsExternalCredential(EExternalCredentialType CredentialType);

		bool IsInitialized() const { return bIsInitialized; }
		bool IsConnected() const { return bIsConnected; }
		bool IsLoggedIn() const { return bIsLoggedIn; }
		bool NeedsLogin() const { return true; }
		const FString& GetUserId() const { return UserId; }
		const FString& GetDisplayName() const { return DisplayName; }

		// Opaque handle accessors used by the capability classes. Each returns the EOS_* handle / id as a
		// void* so this header stays SDK-free; the capability .cpp reinterpret_casts it back.
		void* GetPlatformHandle() const;
		void* GetAchievementsHandle() const;
		void* GetLeaderboardsHandle() const;
		void* GetStatsHandle() const;
		void* GetPlayerDataStorageHandle() const;
		void* GetLobbyHandle() const;
		void* GetEcomHandle() const;
		void* GetConnectHandle() const;
		void* GetUserInfoHandle() const;
		void* GetP2PHandle() const;
		void* GetFriendsHandle() const;
		void* GetPresenceHandle() const;

		void* GetEpicAccountId() const;
		void* GetProductUserId() const;

		bool AreDefinitionsLoaded() const { return bDefinitionsLoaded; }

		// Opaque accessors over the cached EOS definitions. The achievements capability iterates the
		// returned EOS_Achievements_DefinitionV2* pointers; the leaderboards capability looks up an
		// EOS_Leaderboards_Definition* by id. Both cast the void* back in their own .cpp.
		TArray<const void*> GetAchievementDefinitionPtrs() const;
		const void* FindLeaderboardDefinition(const FString& LeaderboardId) const;

		// Cloud-storage path helpers (shared by the cloud-storage capability).
		FString GetFullLocalPath(const FString& RelativePath) const;
		void SetTempStoragePath(const FString& InPath);
		const FString& GetTempStoragePath() const { return TempStoragePath; }

		// Cloud sync entry points (driven from login + shutdown). Implemented by FEOSCloudStorage but
		// invoked through this core, so the core holds settable hooks the capability wires up. Kept in the
		// core because the sync ordering is owned by the login / shutdown sequence: SyncFromCloudHook fires
		// at the end of CompleteAuthentication, SyncToCloudHook fires (and is ticked to completion) at the
		// start of Shutdown. Moving the orchestration out of the core would break that ordering.
		TFunction<void(TFunction<void(const FGamingServiceResult&)>)> SyncFromCloudHook;
		TFunction<void(TFunction<void(const FGamingServiceResult&)>)> SyncToCloudHook;

		// Matchmaking notification hooks owned by FEOSMatchmaking (lobby invite-accepted + member-status).
		// The matchmaking capability conceptually owns them, but their registration needs the lobby handle +
		// ProductUserId that only exist after login, and their teardown must happen during core Shutdown. So
		// the core fires these hooks at the right points in the login / shutdown sequence and matchmaking
		// does the actual EOS_Lobby_AddNotify* / RemoveNotify* work (and fires its own sinks directly).
		// RegisterMatchmakingNotificationsHook fires at the end of CompleteAuthentication;
		// UnregisterMatchmakingNotificationsHook fires during Shutdown.
		TFunction<void()> RegisterMatchmakingNotificationsHook;
		TFunction<void()> UnregisterMatchmakingNotificationsHook;

		// Achievement notification hooks owned by FEOSAchievements, fired at the same two points and for
		// the same reason as the matchmaking pair above: EOS_Achievements_AddNotifyAchievementsUnlockedV2
		// needs the achievements handle and the ProductUserId, which only exist after login, and the
		// matching RemoveNotify must run before the platform goes away.
		TFunction<void()> RegisterAchievementsNotificationsHook;
		TFunction<void()> UnregisterAchievementsNotificationsHook;

	private:
		struct FImpl;
		TPimplPtr<FImpl> Impl;

		bool InitializeEOSPlatform(const FEOSInitOptions& EOSOpts);
		void ShutdownEOSPlatform();
		void Shutdown();

		// Login flow. EOS handles / ids are passed as opaque void* so this header stays SDK-free; the .cpp
		// casts them back to EOS_EpicAccountId / EOS_ContinuanceToken / EOS_ProductUserId. AuthCtx is the
		// private login context (FEOSCorePlatformLoginCtx), forward-declared and only used in the .cpp.
		void AuthLogin(const FGamingServiceLoginParams& Params, TFunction<void(const FGamingServiceResult&)> Callback);
		void ConnectLogin(void* EpicAccountId, TFunction<void(const FGamingServiceResult&)> Callback);

		// Shared tail of every Connect login, whoever supplied the credential. CredentialType is an
		// EOS_EExternalCredentialType passed as int32 so this header stays SDK-free.
		void ConnectLoginWithToken(const FString& Token, int32 CredentialType, const FString& InDisplayName,
		                           TFunction<void(const FGamingServiceResult&)> Callback);
		void CreateUser(struct FEOSCorePlatformLoginCtx* Ctx, void* ContinuanceToken) const;
		void CompleteAuthentication(void* InProductUserId, struct FEOSCorePlatformLoginCtx* AuthCtx);

		// Resolves the logged-in account's display name into DisplayName. Best-effort: always calls
		// OnComplete, success or not, so the login sequence never stalls on it.
		void QueryDisplayName(TFunction<void()> OnComplete);

		void LoadAchievementDefinitions(TFunction<void(const bool&)> OnComplete);
		void LoadLeaderboardDefinitions(TFunction<void(const bool&)> OnComplete);
		void OnAchievementDefinitionsLoaded();
		void OnLeaderboardDefinitionsLoaded();

		bool bIsInitialized = false;
		bool bIsConnected = false;
		bool bIsLoggedIn = false;
		bool bEOSSDKInitialized = false;
		FString UserId;
		FString DisplayName;

		FString TempStoragePath;
		bool bDefinitionsLoaded = false;
	};
}

#endif // GS_WITH_EOS

#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "DataTypes/ConnectTypes.h"
#include "DataTypes/LoginTypes.h"
#include "DataTypes/SessionTypes.h"
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

		// Platform lifecycle.
		void InitializePlatform();
		void DestroyPlatform();
		void Tick();

		// Login / identity (Auth + Connect flow).
		void Login(const FGamingServiceLoginParams& Params, TFunction<void(const FGamingServiceResult&)> Callback);

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
		void* GetSessionsHandle() const;
		void* GetEcomHandle() const;

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

		// Session bookkeeping shared between matchmaking and the connection-string helper.
		bool bIsInSession = false;
		bool bIsSessionHost = false;
		FString CurrentSessionName;
		FSessionSettings CurrentSessionSettings;

		FString GetSessionConnectionString() const;

		// Cloud sync entry points (driven from login + shutdown). Implemented by FEOSCloudStorage but
		// invoked through this core, so the core holds settable hooks the capability wires up. Kept in the
		// core because the sync ordering is owned by the login / shutdown sequence: SyncFromCloudHook fires
		// at the end of CompleteAuthentication, SyncToCloudHook fires (and is ticked to completion) at the
		// start of Shutdown. Moving the orchestration out of the core would break that ordering.
		TFunction<void(TFunction<void(const FGamingServiceResult&)>)> SyncFromCloudHook;
		TFunction<void(TFunction<void(const FGamingServiceResult&)>)> SyncToCloudHook;

		// Session-invite notification hooks owned by FEOSMatchmaking. The matchmaking capability conceptually
		// owns the invite notification, but its registration needs the sessions handle + ProductUserId that
		// only exist after login, and its teardown must happen during core Shutdown. So the core fires these
		// hooks at the right points in the login / shutdown sequence and matchmaking does the actual
		// EOS_Sessions_AddNotifySessionInviteAccepted / Remove work (and fires its own OnLobbyInviteAccepted
		// sink directly). RegisterSessionInviteNotificationHook fires at the end of CompleteAuthentication;
		// UnregisterSessionInviteNotificationHook fires during Shutdown.
		TFunction<void()> RegisterSessionInviteNotificationHook;
		TFunction<void()> UnregisterSessionInviteNotificationHook;

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
		void CreateUser(struct FEOSCorePlatformLoginCtx* Ctx, void* ContinuanceToken) const;
		void CompleteAuthentication(void* InProductUserId, struct FEOSCorePlatformLoginCtx* AuthCtx);

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

#endif // USE_EOS

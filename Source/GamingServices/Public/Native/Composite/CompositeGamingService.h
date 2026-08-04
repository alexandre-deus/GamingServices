#pragma once

#include "CoreMinimal.h"
#include "Native/GamingBackend.h"
#include "Native/IGamingService.h"

namespace GamingServices
{
	class FCompositeUser;
	class FCompositeMatchmaking;

	/**
	 * A gaming service assembled from more than one live backend.
	 *
	 * This exists ONLY when the configuration actually asks for cross-backend behaviour — an AuthBackend
	 * different from the primary, or a capability override pointing elsewhere. A single-backend setup is
	 * handed back unwrapped by the factory, so it pays nothing for this and behaves exactly as it did
	 * when the backend was picked at build time.
	 *
	 * What it does:
	 *   - Owns every configured backend and drives all of their lifecycles (init / tick / teardown), so
	 *     the identity backend is pumped often enough to deliver its auth callbacks.
	 *   - Routes each capability to the backend the config names for it, falling back to the primary
	 *     whenever the routed backend does not implement it.
	 *   - Replaces the user capability with FCompositeUser when an auth backend is configured, which is
	 *     what turns "Steam signs in" plus "EOS runs the session" into one login.
	 *
	 * Session-shaped capabilities (P2P transport, matchmaking) always come from the primary: they must
	 * agree on the same identity namespace as each other.
	 */
	class GAMINGSERVICES_API FCompositeGamingService final : public IGamingService
	{
	public:
		/** A backend instance plus the enum naming it. Ordered, primary first. */
		struct FBackendEntry
		{
			EGamingBackend Backend = EGamingBackend::None;
			TUniquePtr<IGamingService> Service;
		};

		FCompositeGamingService(const FGamingServicesRuntimeConfig& InConfig, TArray<FBackendEntry> InBackends);
		virtual ~FCompositeGamingService() override;

		virtual void InitializePlatform(const FGamingServiceConnectParams& Params) override;
		virtual void DestroyPlatform() override;
		virtual void Tick() override;
		virtual bool IsInitialized() const override;

		virtual IAchievementsService*   GetAchievements()   const override;
		virtual IEntitlementsService*   GetEntitlements()   const override;
		virtual ILeaderboardsService*   GetLeaderboards()   const override;
		virtual IStatsService*          GetStats()          const override;
		virtual ICloudStorageService*   GetCloudStorage()   const override;
		virtual IRemoteSettingsService* GetRemoteSettings() const override;
		virtual IMatchmakingService*    GetMatchmaking()    const override;
		virtual IUserService*           GetUser()           const override;
		virtual IFriendsService*        GetFriends()        const override;
		virtual IP2PTransport*          GetP2PTransport()   override;

		virtual EGamingBackend GetBackend() const override;

		virtual IExternalAuthProvider* GetExternalAuthProvider() const override;
		virtual IExternalAuthConsumer* GetExternalAuthConsumer() const override;
		virtual IInviteTransport* GetInviteTransport() const override;

		/** The backend that owns the session. Never null once constructed. */
		IGamingService* GetPrimaryService() const;

		/** A specific live backend, or null when it is not part of this composite. */
		IGamingService* GetBackendService(EGamingBackend Backend) const;

	private:
		/**
		 * Resolves a capability to the backend the config routes it to, falling back to the primary when
		 * that backend does not implement it. Taking the accessor as a member pointer keeps one
		 * implementation for all eight capabilities instead of eight near-identical ones.
		 */
		template <typename TCapability>
		TCapability* RouteCapability(EGamingCapability Capability, TCapability* (IGamingService::*Accessor)() const) const;

		FGamingServicesRuntimeConfig Config;
		TArray<FBackendEntry> Backends;
		EGamingBackend Primary = EGamingBackend::None;

		/** Present only when an auth backend is configured; otherwise the user capability is routed normally. */
		TUniquePtr<FCompositeUser> User;

		/**
		 * Present only when the identity backend can carry invites for the primary's sessions. Wraps the
		 * primary's matchmaking; without it, matchmaking is routed unwrapped.
		 */
		TUniquePtr<FCompositeMatchmaking> Matchmaking;
	};
}

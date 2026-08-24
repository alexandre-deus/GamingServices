#pragma once

#include "CoreMinimal.h"
#include "Native/IGamingService.h"

#if defined(GS_WITH_EOS)

#include "Native/AchievementProgressStore.h"
#include "Native/RemoteSettingsStore.h"

namespace GamingServices
{
	class FEOSPlatformCore;
	class FEOSAchievements;
	class FEOSEntitlements;
	class FEOSLeaderboards;
	class FEOSStats;
	class FEOSCloudStorage;
	class FEOSMatchmaking;
	class FEOSUser;
	class FEOSExternalAuth;
	class FEOSFriends;

	/**
	 * EOS (Epic Online Services) backend.
	 *
	 * Owns the shared platform core plus one instance per capability class, and a FRemoteSettingsStore
	 * layered over the cloud-storage capability. The constructor is trivial; all SDK bring-up happens in
	 * InitializePlatform(), which also wires the core's notification hooks to the matchmaking / user
	 * capability sinks.
	 *
	 * Implements every capability. Avatar is part of the User capability; the EOS C++ SDK exposes no
	 * native avatar texture API, so FEOSUser::GetAvatar* currently return nullptr pending a source for
	 * Epic-account avatars (see FEOSUser).
	 */
	class GAMINGSERVICES_API FEOSGamingService final : public IGamingService
	{
	public:
		FEOSGamingService();
		virtual ~FEOSGamingService() override;

		virtual void InitializePlatform(const FGamingServiceConnectParams& Params) override;
		virtual void DestroyPlatform() override;
		virtual void Tick() override;
		virtual bool IsInitialized() const override;

		// EOS implements every capability except avatar (HasAvatars stays false from the base).
		virtual IAchievementsService*   GetAchievements()   const override;
		virtual IAchievementProgressService* GetAchievementProgress() const override;
		virtual IEntitlementsService*   GetEntitlements()   const override;
		virtual ILeaderboardsService*   GetLeaderboards()   const override;
		virtual IStatsService*          GetStats()          const override;
		virtual ICloudStorageService*   GetCloudStorage()   const override;
		virtual IRemoteSettingsService* GetRemoteSettings() const override;
		virtual IMatchmakingService*    GetMatchmaking()    const override;
		virtual IUserService*           GetUser()           const override;
		virtual IP2PTransport*          GetP2PTransport()   override;

		/**
		 * Null until the local user has an EpicAccountId. Friends and Presence are Epic Account Services,
		 * so a Connect-only sign-in (Steam authenticating into EOS) genuinely has no social graph here —
		 * reporting the capability as absent is the honest answer, and it makes the composite fall through
		 * to the identity backend's friend list instead.
		 */
		virtual IFriendsService*        GetFriends()        const override;

		virtual EGamingBackend GetBackend() const override { return EGamingBackend::EpicOnlineServices; }

		/** EOS can be signed into with another platform's credential (Connect takes a Steam session ticket). */
		virtual IExternalAuthConsumer* GetExternalAuthConsumer() const override;

	private:
		TUniquePtr<FEOSPlatformCore> Core;
		TUniquePtr<FEOSExternalAuth> ExternalAuth;
		TUniquePtr<IP2PTransport> P2PTransport;
		TUniquePtr<FEOSAchievements> Achievements;
		TUniquePtr<FEOSEntitlements> Entitlements;
		TUniquePtr<FEOSStats> Stats;
		TUniquePtr<FEOSLeaderboards> Leaderboards;
		TUniquePtr<FEOSCloudStorage> CloudStorage;
		TUniquePtr<FEOSMatchmaking> Matchmaking;
		TUniquePtr<FEOSUser> User;
		TUniquePtr<FRemoteSettingsStore> RemoteSettings;
		// Declared after Achievements and Stats: it holds references to both, and members destroy in
		// reverse declaration order.
		TUniquePtr<FAchievementProgressStore> AchievementProgress;
		// Declared last on purpose: Friends holds an IMatchmakingService& (it invites to the current
		// lobby), and members destroy in reverse declaration order.
		TUniquePtr<FEOSFriends> Friends;
	};
}

#endif // GS_WITH_EOS

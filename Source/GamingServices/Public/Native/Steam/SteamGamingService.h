#pragma once

#include "CoreMinimal.h"
#include "Native/IGamingService.h"

class FRemoteSettingsStore;

namespace GamingServices
{
	class FSteamPlatformCore;
	class FSteamAchievements;
	class FSteamEntitlements;
	class FSteamLeaderboards;
	class FSteamStats;
	class FSteamCloudStorage;
	class FSteamMatchmaking;
	class FSteamUser;
	class FSteamExternalAuth;
	class FSteamInviteTransport;
	class FSteamFriends;

	/**
	 * Steamworks backend. Owns the shared FSteamPlatformCore plus one instance per capability,
	 * and answers capability queries by handing back the matching interface pointer.
	 *
	 * Construction is trivial (no SDK work); InitializePlatform() boots the core. Each capability owns
	 * its own Steam callbacks and fires its own notification sinks directly.
	 */
	class GAMINGSERVICES_API FSteamGamingService : public IGamingService
	{
	public:
		FSteamGamingService();
		virtual ~FSteamGamingService() override;

		virtual void InitializePlatform(const FGamingServiceConnectParams& Params) override;
		virtual void DestroyPlatform() override;
		virtual void Tick() override;
		virtual bool IsInitialized() const override;

		// Steam implements every capability.
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

		virtual EGamingBackend GetBackend() const override { return EGamingBackend::Steamworks; }

		/** Steam can vouch for the local user to another backend (EOS Connect takes its session ticket). */
		virtual IExternalAuthProvider* GetExternalAuthProvider() const override;

		/** Steam's friends list and overlay can carry another backend's session id as an invite. */
		virtual IInviteTransport* GetInviteTransport() const override;

	private:
		TUniquePtr<FSteamPlatformCore> Core;
		TUniquePtr<FSteamExternalAuth> ExternalAuth;
		TUniquePtr<FSteamInviteTransport> InviteTransport;
		TUniquePtr<IP2PTransport> P2PTransport;
		TUniquePtr<FSteamAchievements> Achievements;
		TUniquePtr<FSteamEntitlements> Entitlements;
		TUniquePtr<FSteamLeaderboards> Leaderboards;
		TUniquePtr<FSteamStats> Stats;
		TUniquePtr<FSteamCloudStorage> CloudStorage;
		// User is declared before Matchmaking: Matchmaking holds a FSteamUser&, and members destroy in
		// reverse declaration order, so Matchmaking must be torn down before the User it references.
		TUniquePtr<FSteamUser> User;
		TUniquePtr<FSteamMatchmaking> Matchmaking;
		TUniquePtr<FRemoteSettingsStore> RemoteSettings;
		// Declared last on purpose: Friends holds a FSteamInviteTransport& (it invites to whatever session
		// the transport has published), and members destroy in reverse declaration order.
		TUniquePtr<FSteamFriends> Friends;
	};
}

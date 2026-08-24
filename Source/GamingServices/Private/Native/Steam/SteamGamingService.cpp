#ifdef GS_WITH_STEAM

#include "Native/Steam/SteamGamingService.h"

#include "Native/AchievementProgressStore.h"
#include "Native/RemoteSettingsStore.h"
#include "Native/Steam/SteamPlatformCore.h"
#include "Native/Steam/Interfaces/SteamAchievements.h"
#include "Native/Steam/Interfaces/SteamEntitlements.h"
#include "Native/Steam/Interfaces/SteamLeaderboards.h"
#include "Native/Steam/Interfaces/SteamStats.h"
#include "Native/Steam/Interfaces/SteamCloudStorage.h"
#include "Native/Steam/Interfaces/SteamMatchmaking.h"
#include "Native/Steam/Interfaces/SteamUser.h"
#include "Native/Steam/Interfaces/SteamP2PTransport.h"
#include "Native/Steam/Interfaces/SteamExternalAuth.h"
#include "Native/Steam/Interfaces/SteamInviteTransport.h"
#include "Native/Steam/Interfaces/SteamFriends.h"

namespace GamingServices
{
	FSteamGamingService::FSteamGamingService()
	{
		// Trivial construction only — no SDK work happens until InitializePlatform().
		Core = MakeUnique<FSteamPlatformCore>();
		User = MakeUnique<FSteamUser>(*Core);
		Matchmaking = MakeUnique<FSteamMatchmaking>(*Core, *User);
		Achievements = MakeUnique<FSteamAchievements>(*Core);
		Entitlements = MakeUnique<FSteamEntitlements>(*Core);
		Leaderboards = MakeUnique<FSteamLeaderboards>(*Core);
		Stats = MakeUnique<FSteamStats>(*Core);
		CloudStorage = MakeUnique<FSteamCloudStorage>(*Core);
		RemoteSettings = MakeUnique<FRemoteSettingsStore>(*CloudStorage);
		AchievementProgress = MakeUnique<FAchievementProgressStore>(*Achievements, Stats.Get(),
		                                                           EGamingBackend::Steamworks);
		ExternalAuth = MakeUnique<FSteamExternalAuth>(*Core);
		InviteTransport = MakeUnique<FSteamInviteTransport>(*Core);
		Friends = MakeUnique<FSteamFriends>(*Core, *InviteTransport);
	}

	FSteamGamingService::~FSteamGamingService() = default;

	void FSteamGamingService::InitializePlatform(const FGamingServiceConnectParams& Params)
	{
		// Steamworks has no per-instance credential overrides (FSteamworksInitOptions is empty).
		Core->InitializePlatform();
	}

	void FSteamGamingService::DestroyPlatform()
	{
		// Tear the transport down before SteamAPI shuts down: its destructor closes sockets/connections.
		P2PTransport.Reset();
		Core->DestroyPlatform();
	}

	void FSteamGamingService::Tick()
	{
		Core->Tick();
		Matchmaking->Tick();
	}

	bool FSteamGamingService::IsInitialized() const
	{
		return Core->IsInitialized();
	}

	IAchievementsService*   FSteamGamingService::GetAchievements()   const { return Achievements.Get(); }
	IAchievementProgressService* FSteamGamingService::GetAchievementProgress() const { return AchievementProgress.Get(); }
	IEntitlementsService*   FSteamGamingService::GetEntitlements()   const { return Entitlements.Get(); }
	ILeaderboardsService*   FSteamGamingService::GetLeaderboards()   const { return Leaderboards.Get(); }
	IStatsService*          FSteamGamingService::GetStats()          const { return Stats.Get(); }
	ICloudStorageService*   FSteamGamingService::GetCloudStorage()   const { return CloudStorage.Get(); }
	IRemoteSettingsService* FSteamGamingService::GetRemoteSettings() const { return RemoteSettings.Get(); }
	IMatchmakingService*    FSteamGamingService::GetMatchmaking()    const { return Matchmaking.Get(); }
	IUserService*           FSteamGamingService::GetUser()           const { return User.Get(); }
	IFriendsService*        FSteamGamingService::GetFriends()        const { return Friends.Get(); }

	IExternalAuthProvider* FSteamGamingService::GetExternalAuthProvider() const { return ExternalAuth.Get(); }

	IInviteTransport* FSteamGamingService::GetInviteTransport() const { return InviteTransport.Get(); }

	IP2PTransport* FSteamGamingService::GetP2PTransport()
	{
		// Steam networking is available as soon as SteamAPI is up (no explicit login step), so create
		// the transport lazily on first request.
		if (!P2PTransport && IsInitialized())
		{
			P2PTransport = MakeUnique<FSteamP2PTransport>();
		}
		return P2PTransport.Get();
	}
}

#endif // GS_WITH_STEAM

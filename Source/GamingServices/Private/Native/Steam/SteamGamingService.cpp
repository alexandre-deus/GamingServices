#ifdef USE_STEAMWORKS

#include "Native/Steam/SteamGamingService.h"

#include "Native/RemoteSettingsStore.h"
#include "Native/Steam/SteamPlatformCore.h"
#include "Native/Steam/Interfaces/SteamAchievements.h"
#include "Native/Steam/Interfaces/SteamEntitlements.h"
#include "Native/Steam/Interfaces/SteamLeaderboards.h"
#include "Native/Steam/Interfaces/SteamStats.h"
#include "Native/Steam/Interfaces/SteamCloudStorage.h"
#include "Native/Steam/Interfaces/SteamMatchmaking.h"
#include "Native/Steam/Interfaces/SteamUser.h"

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
	}

	FSteamGamingService::~FSteamGamingService() = default;

	void FSteamGamingService::InitializePlatform()
	{
		Core->InitializePlatform();
	}

	void FSteamGamingService::DestroyPlatform()
	{
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
	IEntitlementsService*   FSteamGamingService::GetEntitlements()   const { return Entitlements.Get(); }
	ILeaderboardsService*   FSteamGamingService::GetLeaderboards()   const { return Leaderboards.Get(); }
	IStatsService*          FSteamGamingService::GetStats()          const { return Stats.Get(); }
	ICloudStorageService*   FSteamGamingService::GetCloudStorage()   const { return CloudStorage.Get(); }
	IRemoteSettingsService* FSteamGamingService::GetRemoteSettings() const { return RemoteSettings.Get(); }
	IMatchmakingService*    FSteamGamingService::GetMatchmaking()    const { return Matchmaking.Get(); }
	IUserService*           FSteamGamingService::GetUser()           const { return User.Get(); }
}

#endif // USE_STEAMWORKS

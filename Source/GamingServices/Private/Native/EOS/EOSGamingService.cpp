#if defined(USE_EOS)

#include "Native/EOS/EOSGamingService.h"
#include "Native/EOS/EOSPlatformCore.h"
#include "Native/EOS/Interfaces/EOSAchievements.h"
#include "Native/EOS/Interfaces/EOSEntitlements.h"
#include "Native/EOS/Interfaces/EOSStats.h"
#include "Native/EOS/Interfaces/EOSLeaderboards.h"
#include "Native/EOS/Interfaces/EOSCloudStorage.h"
#include "Native/EOS/Interfaces/EOSMatchmaking.h"
#include "Native/EOS/Interfaces/EOSUser.h"

namespace GamingServices
{
	FEOSGamingService::FEOSGamingService()
	{
		// Trivial construction only — no SDK work happens until InitializePlatform().
		// Construction order respects dependencies: Stats before Leaderboards (which references it),
		// and CloudStorage before RemoteSettings (which is layered over it).
		Core = MakeUnique<FEOSPlatformCore>();
		Achievements = MakeUnique<FEOSAchievements>(*Core);
		Entitlements = MakeUnique<FEOSEntitlements>(*Core);
		Stats = MakeUnique<FEOSStats>(*Core);
		Leaderboards = MakeUnique<FEOSLeaderboards>(*Core, *Stats);
		CloudStorage = MakeUnique<FEOSCloudStorage>(*Core);
		Matchmaking = MakeUnique<FEOSMatchmaking>(*Core);
		User = MakeUnique<FEOSUser>(*Core);
		RemoteSettings = MakeUnique<FRemoteSettingsStore>(*CloudStorage);
	}

	FEOSGamingService::~FEOSGamingService() = default;

	void FEOSGamingService::InitializePlatform()
	{
		Core->InitializePlatform();
	}

	void FEOSGamingService::DestroyPlatform()
	{
		Core->DestroyPlatform();
	}

	void FEOSGamingService::Tick()
	{
		Core->Tick();
	}

	bool FEOSGamingService::IsInitialized() const
	{
		return Core->IsInitialized();
	}

	IAchievementsService*   FEOSGamingService::GetAchievements()   const { return Achievements.Get(); }
	IEntitlementsService*   FEOSGamingService::GetEntitlements()   const { return Entitlements.Get(); }
	ILeaderboardsService*   FEOSGamingService::GetLeaderboards()   const { return Leaderboards.Get(); }
	IStatsService*          FEOSGamingService::GetStats()          const { return Stats.Get(); }
	ICloudStorageService*   FEOSGamingService::GetCloudStorage()   const { return CloudStorage.Get(); }
	IRemoteSettingsService* FEOSGamingService::GetRemoteSettings() const { return RemoteSettings.Get(); }
	IMatchmakingService*    FEOSGamingService::GetMatchmaking()    const { return Matchmaking.Get(); }
	IUserService*           FEOSGamingService::GetUser()           const { return User.Get(); }
}

#endif // USE_EOS

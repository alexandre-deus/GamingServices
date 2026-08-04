#if defined(GS_WITH_EOS)

#include "Native/EOS/EOSGamingService.h"
#include "Native/EOS/EOSPlatformCore.h"
#include "Native/EOS/Interfaces/EOSAchievements.h"
#include "Native/EOS/Interfaces/EOSEntitlements.h"
#include "Native/EOS/Interfaces/EOSStats.h"
#include "Native/EOS/Interfaces/EOSLeaderboards.h"
#include "Native/EOS/Interfaces/EOSCloudStorage.h"
#include "Native/EOS/Interfaces/EOSMatchmaking.h"
#include "Native/EOS/Interfaces/EOSUser.h"
#include "Native/EOS/Interfaces/EOSP2PTransport.h"
#include "Native/EOS/Interfaces/EOSExternalAuth.h"
#include "Native/EOS/Interfaces/EOSFriends.h"

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
		Friends = MakeUnique<FEOSFriends>(*Core, *Matchmaking);
		RemoteSettings = MakeUnique<FRemoteSettingsStore>(*CloudStorage);
		ExternalAuth = MakeUnique<FEOSExternalAuth>(*Core);
	}

	FEOSGamingService::~FEOSGamingService() = default;

	void FEOSGamingService::InitializePlatform(const FGamingServiceConnectParams& Params)
	{
		Core->InitializePlatform(Params.EOS);
	}

	void FEOSGamingService::DestroyPlatform()
	{
		// Tear the P2P transport down first: its destructor removes EOS notifications and closes
		// connections, which must happen while the platform (and its P2P interface) is still alive.
		P2PTransport.Reset();
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

	IFriendsService* FEOSGamingService::GetFriends() const
	{
		// Gated on IsAvailable rather than mere existence, so the capability only appears once there is an
		// EpicAccountId behind it. Before login, and for a Connect-only user, this stays null.
		return (Friends && Friends->IsAvailable()) ? Friends.Get() : nullptr;
	}

	IExternalAuthConsumer* FEOSGamingService::GetExternalAuthConsumer() const { return ExternalAuth.Get(); }

	IP2PTransport* FEOSGamingService::GetP2PTransport()
	{
		// Lazily created once we can address peers: needs the P2P interface handle and the local
		// ProductUserId, both of which exist only after login completes.
		if (!P2PTransport)
		{
			void* P2PHandle = Core->GetP2PHandle();
			void* LocalUser = Core->GetProductUserId();
			if (P2PHandle && LocalUser)
			{
				P2PTransport = MakeUnique<FEOSP2PTransport>(P2PHandle, LocalUser);
			}
		}
		return P2PTransport.Get();
	}
}

#endif // GS_WITH_EOS

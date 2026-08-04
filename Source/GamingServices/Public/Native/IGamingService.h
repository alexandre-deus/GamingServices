#pragma once

#include "CoreMinimal.h"
#include "DataTypes/ConnectTypes.h"
#include "Native/GamingBackend.h"
#include "Native/GamingCapability.h"

class IAchievementsService;
class IExternalAuthProvider;
class IExternalAuthConsumer;
class IInviteTransport;
class IEntitlementsService;
class ILeaderboardsService;
class IStatsService;
class ICloudStorageService;
class IRemoteSettingsService;
class IMatchmakingService;
class IUserService;
class IFriendsService;
class IP2PTransport;

/**
 * Native (non-UObject) interface for the OOP gaming-service abstraction.
 *
 * The service owns platform lifecycle plus one accessor per capability interface. A backend overrides
 * only the accessors for the capabilities it implements; every other accessor keeps the base's
 * nullptr. So "not supported" is a plain null at the source:
 *
 *     if (IMatchmakingService* MM = Service->GetMatchmaking())
 *     {
 *         MM->CreateSession(Settings, Callback);
 *     }
 *
 * GetCapabilities() is derived from these accessors, so the struct and the accessors are always one
 * source of truth.
 */
class GAMINGSERVICES_API IGamingService
{
public:
	virtual ~IGamingService() = default;

	// Platform lifecycle. Params are optional per-field overrides on top of the ini config: any field
	// left empty falls back to [GamingServices.<Backend>] in Game.ini. Tests use this to run several
	// local instances against different backend credentials without touching the config.
	virtual void InitializePlatform(const FGamingServiceConnectParams& Params = FGamingServiceConnectParams()) {}
	virtual void DestroyPlatform() {}
	virtual void Tick() = 0;
	virtual bool IsInitialized() const = 0;

	// Capability accessors — a backend overrides the ones it implements; default null = unsupported.
	virtual IAchievementsService*   GetAchievements()   const { return nullptr; }
	virtual IEntitlementsService*   GetEntitlements()   const { return nullptr; }
	virtual ILeaderboardsService*   GetLeaderboards()   const { return nullptr; }
	virtual IStatsService*          GetStats()          const { return nullptr; }
	virtual ICloudStorageService*   GetCloudStorage()   const { return nullptr; }
	virtual IRemoteSettingsService* GetRemoteSettings() const { return nullptr; }
	virtual IMatchmakingService*    GetMatchmaking()    const { return nullptr; }
	virtual IUserService*           GetUser()           const { return nullptr; }

	/**
	 * The local user's social graph, or null where it is unreadable. Notably null on EOS for a user who
	 * signed in through Connect only: the Friends interface is Epic Account Services and needs an
	 * EpicAccountId, which an externally-authenticated user does not have.
	 */
	virtual IFriendsService*        GetFriends()        const { return nullptr; }

	/**
	 * P2P networking transport for this backend (Steam / EOS), or null when unsupported or not ready
	 * (e.g. before login). Created lazily once the platform can address peers. SDK-free interface, so
	 * this stays out of the platform SDKs; the netdriver and tests drive it directly.
	 */
	virtual IP2PTransport* GetP2PTransport() { return nullptr; }

	/** Which platform this service speaks to. A composite reports its primary backend. */
	virtual EGamingBackend GetBackend() const { return EGamingBackend::None; }

	/**
	 * Cross-backend authentication. A backend that can vouch for the local user to another one returns
	 * a provider (Steam); a backend that can be logged into with someone else's credential returns a
	 * consumer (EOS). The composite service pairs them so a Steam sign-in yields an EOS session.
	 */
	virtual IExternalAuthProvider* GetExternalAuthProvider() const { return nullptr; }
	virtual IExternalAuthConsumer* GetExternalAuthConsumer() const { return nullptr; }

	/**
	 * This platform's invite system, when it can carry a session id belonging to another backend.
	 * Null on backends with no such channel. The composite pairs it with the primary's matchmaking so
	 * friends on the identity platform can be invited into a session the primary owns.
	 */
	virtual IInviteTransport* GetInviteTransport() const { return nullptr; }

	/** Flat capability snapshot, derived from the accessors above (single source of truth). */
	FGamingServiceCapabilities GetCapabilities() const
	{
		FGamingServiceCapabilities Caps;
		Caps.bAchievements   = GetAchievements()   != nullptr;
		Caps.bEntitlements   = GetEntitlements()   != nullptr;
		Caps.bLeaderboards   = GetLeaderboards()   != nullptr;
		Caps.bStats          = GetStats()          != nullptr;
		Caps.bCloudStorage   = GetCloudStorage()   != nullptr;
		Caps.bRemoteSettings = GetRemoteSettings() != nullptr;
		Caps.bMatchmaking    = GetMatchmaking()    != nullptr;
		Caps.bUser           = GetUser()           != nullptr;
		Caps.bFriends        = GetFriends()        != nullptr;
		return Caps;
	}
};

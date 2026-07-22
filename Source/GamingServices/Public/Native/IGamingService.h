#pragma once

#include "CoreMinimal.h"
#include "DataTypes/ConnectTypes.h"
#include "Native/GamingCapability.h"

class IAchievementsService;
class IEntitlementsService;
class ILeaderboardsService;
class IStatsService;
class ICloudStorageService;
class IRemoteSettingsService;
class IMatchmakingService;
class IUserService;
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
	 * P2P networking transport for this backend (Steam / EOS), or null when unsupported or not ready
	 * (e.g. before login). Created lazily once the platform can address peers. SDK-free interface, so
	 * this stays out of the platform SDKs; the netdriver and tests drive it directly.
	 */
	virtual IP2PTransport* GetP2PTransport() { return nullptr; }

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
		return Caps;
	}
};

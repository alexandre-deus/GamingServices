#pragma once

#include "CoreMinimal.h"
#include "Native/GamingCapability.h"

class IAchievementsService;
class IEntitlementsService;
class ILeaderboardsService;
class IStatsService;
class ICloudStorageService;
class IRemoteSettingsService;
class IMatchmakingService;
class IUserService;

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

	// Platform lifecycle.
	virtual void InitializePlatform() {}
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

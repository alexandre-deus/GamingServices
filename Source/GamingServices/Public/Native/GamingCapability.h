#pragma once

#include "CoreMinimal.h"
#include "GamingCapability.generated.h"

/**
 * Identifies a single capability that a gaming-service backend may or may not support.
 * Used by the native query path (IGamingService::GetCapability<T>) and by HasCapability().
 */
UENUM(BlueprintType)
enum class EGamingCapability : uint8
{
	Achievements,
	Entitlements,
	Leaderboards,
	Stats,
	CloudStorage,
	RemoteSettings,
	Matchmaking,
	User
};

/**
 * Flat, value-type description of everything a backend supports. Returned by the "straight up"
 * capabilities query (IGamingService::GetCapabilities / UGamingPlatformSubsystem::GetCapabilities).
 *
 * bAvatar is a sub-capability of User: every real backend exposes user identity, but only some
 * (Steam) expose avatar textures (EOS has no native avatar API).
 */
USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FGamingServiceCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bAchievements = false;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bEntitlements = false;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bLeaderboards = false;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bStats = false;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bCloudStorage = false;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bRemoteSettings = false;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bMatchmaking = false;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Capabilities")
	bool bUser = false;

	/** Returns whether the backend supports at least one capability (i.e. it is a real platform). */
	bool HasAny() const
	{
		return bAchievements || bEntitlements || bLeaderboards || bStats
			|| bCloudStorage || bRemoteSettings || bMatchmaking || bUser;
	}

	/** Returns whether the given capability flag is set. */
	bool Has(EGamingCapability Capability) const
	{
		switch (Capability)
		{
		case EGamingCapability::Achievements:   return bAchievements;
		case EGamingCapability::Entitlements:   return bEntitlements;
		case EGamingCapability::Leaderboards:   return bLeaderboards;
		case EGamingCapability::Stats:          return bStats;
		case EGamingCapability::CloudStorage:   return bCloudStorage;
		case EGamingCapability::RemoteSettings: return bRemoteSettings;
		case EGamingCapability::Matchmaking:    return bMatchmaking;
		case EGamingCapability::User:           return bUser;
		default:                                return false;
		}
	}
};

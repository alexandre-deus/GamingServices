#pragma once

#include "CoreMinimal.h"
#include "DataTypes/ConnectTypes.h"
#include "Native/GamingCapability.h"
#include "GamingBackend.generated.h"

/** Upper bounds on a profile's fixed-size arrays. Raise if a build ever needs more. */
#define GS_MAX_PROFILE_BACKENDS 4
#define GS_MAX_PROFILE_OVERRIDES 8

/** A platform SDK this module can talk to. Which one(s) are live is decided by the compiled-in profile. */
UENUM(BlueprintType)
enum class EGamingBackend : uint8
{
	None UMETA(DisplayName = "None"),
	Steamworks UMETA(DisplayName = "Steamworks"),
	EpicOnlineServices UMETA(DisplayName = "Epic Online Services"),
};

/** Sends one capability to a backend other than the primary one. */
struct FGamingCapabilityOverride
{
	EGamingCapability Capability = EGamingCapability::User;

	/** None marks an unused slot. */
	EGamingBackend Backend = EGamingBackend::None;
};

/**
 * The resolved backend arrangement for this run: a compiled-in FGamingServiceProfile, expanded into
 * runtime containers, with any debug override applied.
 *
 * Configuration lives in code (Native/GamingServiceProfile.h), not in ini. Credentials still come from
 * [GamingServices.EOS] / [GamingServices.Steamworks] — this is about which backends run and how they
 * are wired, which is a property of the build rather than of the installation.
 */
struct GAMINGSERVICES_API FGamingServicesRuntimeConfig
{
	/** For logs: the profile this came from. */
	FString ProfileName;

	/** Preference order. The first entry that is available becomes the primary backend. */
	TArray<EGamingBackend> Backends;

	/** Backend that authenticates the primary. None = the primary authenticates itself. */
	EGamingBackend AuthBackend = EGamingBackend::None;

	/** Per-capability overrides; anything not listed comes from the primary. */
	TArray<FGamingCapabilityOverride> CapabilityOverrides;

	/**
	 * When the auth backend cannot mint a credential (Steam not running, ticket refused), fall back to
	 * the primary backend's own login rather than failing. False makes the auth backend a hard
	 * requirement: the process terminates at init if it does not come up.
	 */
	bool bAllowAuthFallback = true;

	/** Per-backend connection overrides layered on top of each backend's own ini section. */
	FGamingServiceConnectParams ConnectParams;

	/**
	 * The arrangement this build was compiled with (GS_PROFILE), after applying the optional
	 * -GamingBackend= debug override.
	 */
	static FGamingServicesRuntimeConfig Active();

	/** Expands a compile-time profile into a runtime config, without applying any override. */
	static FGamingServicesRuntimeConfig FromProfile(const struct FGamingServiceProfile& Profile);

	/** Every backend named anywhere in this config (ordered list first, then auth, then overrides). */
	TArray<EGamingBackend> GetReferencedBackends() const;

	/** Backend a capability should be served by, given the resolved primary. */
	EGamingBackend GetBackendForCapability(EGamingCapability Capability, EGamingBackend Primary) const;

	/**
	 * Whether this arrangement needs more than one backend wired together. False means a plain single
	 * backend is enough, and the factory hands one back unwrapped — no composite, no routing, no
	 * behavioural difference from a single-backend build.
	 */
	bool RequiresCompositeService(EGamingBackend Primary) const;

	/** One-line description for logging. */
	FString ToString() const;
};

namespace GamingServices
{
	GAMINGSERVICES_API const TCHAR* LexToString(EGamingBackend Backend);

	/** Accepts "EOS"/"Epic"/"EpicOnlineServices", "Steam"/"Steamworks", "None"/"Null" (case-insensitive). */
	GAMINGSERVICES_API bool TryParseGamingBackend(const FString& Text, EGamingBackend& OutBackend);

	/** Whether this build contains the backend's code at all (its SDK was vendored at compile time). */
	GAMINGSERVICES_API bool IsBackendCompiledIn(EGamingBackend Backend);

	/**
	 * Whether the backend can actually be used right now: compiled in AND its shared library resolved
	 * from the common SDK folder. This is the check that makes a missing SDK a runtime non-event.
	 */
	GAMINGSERVICES_API bool IsBackendAvailable(EGamingBackend Backend);
}

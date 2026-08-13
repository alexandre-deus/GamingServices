#pragma once

#include "CoreMinimal.h"
#include "Native/GamingBackend.h"

/**
 * A named backend arrangement, declared in code.
 *
 * This is the source of truth for which backend(s) a build uses and how they are wired together. It is
 * a compile-time constant, so a build variant is a one-token change and a typo is a compile error
 * rather than a silent runtime fallback.
 *
 * Backends are still selected at RUNTIME in the sense that matters — every vendored SDK is compiled in
 * and loaded dynamically, and a backend whose library is missing is skipped in favour of the next
 * preference. The profile decides the arrangement, not what exists.
 */
struct FGamingServiceProfile
{
	/** Shown in logs so a build says out loud which arrangement it was compiled with. */
	const TCHAR* Name = TEXT("Unnamed");

	/**
	 * Ordered preference. The first entry whose SDK library actually loads becomes the primary backend;
	 * later entries are fallbacks. Unused slots are None.
	 */
	EGamingBackend Backends[GS_MAX_PROFILE_BACKENDS] = {};

	/**
	 * Backend that authenticates the primary, if different from it. None means the primary authenticates
	 * itself. Setting this is what turns a single-backend profile into "sign in here, play there".
	 */
	EGamingBackend AuthBackend = EGamingBackend::None;

	/**
	 * When the auth backend cannot vouch for the player (Steam not running, ticket refused), fall back to
	 * the primary's own login instead of failing the sign-in.
	 */
	bool bAllowAuthFallback = true;

	/** Capabilities served by a backend other than the primary. Unused slots have Backend == None. */
	FGamingCapabilityOverride CapabilityOverrides[GS_MAX_PROFILE_OVERRIDES] = {};
};

/**
 * The available arrangements. Add new ones here — a profile is plain data, so a new build variant costs
 * one declaration and no code.
 */
namespace GamingServiceProfiles
{
	/** Everything off. The null backend; every capability honestly reports unsupported. */
	inline constexpr FGamingServiceProfile Disabled
	{
		.Name = TEXT("Disabled"),
	};

	/** Pure EOS: Epic account login, EOS for every capability. */
	inline constexpr FGamingServiceProfile EpicOnly
	{
		.Name = TEXT("EpicOnly"),
		.Backends = { EGamingBackend::EpicOnlineServices },
	};

	/** Pure Steamworks: Steam client identity, Steam for every capability. */
	inline constexpr FGamingServiceProfile SteamOnly
	{
		.Name = TEXT("SteamOnly"),
		.Backends = { EGamingBackend::Steamworks },
	};

	/**
	 * Steam signs the player in, EOS runs the session.
	 *
	 * Steam mints a web-API session ticket, EOS Connect consumes it, and lobbies / P2P / stats /
	 * achievements / cloud saves all go through EOS. No Epic login is ever shown and no Epic account is
	 * created — the EOS ProductUserId is the identity, and the Steam persona rides along as the display
	 * name. Requires a Steam identity provider configured in the EOS Dev Portal.
	 *
	 * If Steam is unavailable (launched outside the client), bAllowAuthFallback lets EOS's own login
	 * take over rather than locking the player out.
	 */
	inline constexpr FGamingServiceProfile SteamAuthIntoEpic
	{
		.Name = TEXT("SteamAuthIntoEpic"),
		.Backends = { EGamingBackend::EpicOnlineServices },
		.AuthBackend = EGamingBackend::Steamworks,
		.bAllowAuthFallback = true,
	};

	/** EOS where available, Steam otherwise. Two independent single-backend paths, never combined. */
	inline constexpr FGamingServiceProfile EpicPreferredSteamFallback
	{
		.Name = TEXT("EpicPreferredSteamFallback"),
		.Backends = { EGamingBackend::EpicOnlineServices, EGamingBackend::Steamworks },
	};
}

/**
 * Which profile a build uses is decided in the build rules, NOT here — this header only declares what
 * the choices are. Nothing in it should need editing to change a build.
 *
 *   - Per target, in a .Target.cs:      ProjectDefinitions.Add("GS_PROFILE=SteamAuthIntoEpic");
 *   - Otherwise GamingServices.Build.cs supplies its DefaultProfile.
 *
 * Both are tracked by UBT, so switching rebuilds what it must, and an unknown profile name is a
 * compile error naming the culprit rather than a silent runtime fallback.
 *
 * There is deliberately no environment-variable switch: UBT caches its module rules evaluation and
 * does not treat the environment as a dependency, so an env-var override silently fails to take effect
 * on an already-built tree.
 */
#ifndef GS_PROFILE
#error "GS_PROFILE is not defined. It is set by GamingServices.Build.cs (DefaultProfile), or pinned per target with ProjectDefinitions.Add(\"GS_PROFILE=<name>\") in a .Target.cs. If you are seeing this, the GamingServices module rules did not run for this target."
#endif

/** The compiled-in arrangement. Resolved at compile time; an unknown GS_PROFILE fails to compile. */
inline constexpr const FGamingServiceProfile& GetActiveGamingServiceProfile()
{
	return GamingServiceProfiles::GS_PROFILE;
}

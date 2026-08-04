#pragma once

#include "CoreMinimal.h"
#include "Native/GamingBackend.h"
#include "Native/IGamingService.h"

namespace GamingServices
{
	/**
	 * Builds the gaming service for this run, from the profile the build was compiled with
	 * (GS_PROFILE, see Native/GamingServiceProfile.h). Every vendored SDK is compiled in and loaded
	 * dynamically, so the profile chooses an arrangement rather than gating what exists.
	 *
	 * Three shapes come out of this, and which one you get is entirely up to the profile:
	 *
	 *   1. Single backend (e.g. EpicOnly, SteamOnly). One entry in Backends, no AuthBackend, no
	 *      overrides. You get that backend's service directly — no wrapper, no routing, no overhead.
	 *
	 *   2. Single backend with fallback (e.g. EpicPreferredSteamFallback). Several entries in Backends;
	 *      the first one whose SDK actually loads wins. Still returned unwrapped.
	 *
	 *   3. Composite (e.g. SteamAuthIntoEpic). Only when the profile wires backends together — an
	 *      AuthBackend other than the primary, or a capability override pointing at another backend.
	 *
	 * When nothing is available (no SDK vendored, no library present, running in the editor) the honest
	 * null backend comes back, and every capability query correctly reports "unsupported".
	 *
	 * The returned service is NOT initialized — the caller owns it and decides whether/when to call
	 * InitializePlatform().
	 */
	GAMINGSERVICES_API TUniquePtr<IGamingService> CreateGamingService();

	/** As above, against an explicit configuration instead of the one in Game.ini. */
	GAMINGSERVICES_API TUniquePtr<IGamingService> CreateGamingService(const FGamingServicesRuntimeConfig& Config);

	/**
	 * One specific backend on its own, with no composition. Returns null when that backend is not
	 * compiled into this build or its SDK library is not loadable on this machine.
	 */
	GAMINGSERVICES_API TUniquePtr<IGamingService> CreateBackendService(EGamingBackend Backend);

	/**
	 * First backend in the config's preference order that is actually usable right now, or None when
	 * none of them are.
	 */
	GAMINGSERVICES_API EGamingBackend ResolvePrimaryBackend(const FGamingServicesRuntimeConfig& Config);
}

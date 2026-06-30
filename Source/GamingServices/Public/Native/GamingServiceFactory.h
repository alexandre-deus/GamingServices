#pragma once

#include "CoreMinimal.h"
#include "Native/IGamingService.h"

namespace GamingServices
{
	/**
	 * Constructs the gaming-service backend selected at build time (USE_STEAMWORKS / USE_EOS),
	 * falling back to the honest null backend when no SDK is configured.
	 *
	 * The returned service is NOT initialized — the caller owns it and decides whether/when to call
	 * InitializePlatform(). The new UGamingPlatformSubsystem keeps it dormant so it never competes
	 * with the legacy module-owned live service for the single platform SDK init.
	 */
	GAMINGSERVICES_API TUniquePtr<IGamingService> CreateGamingService();
}

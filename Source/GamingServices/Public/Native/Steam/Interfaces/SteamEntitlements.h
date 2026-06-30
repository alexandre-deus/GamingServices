#pragma once

#ifdef USE_STEAMWORKS

#include "CoreMinimal.h"
#include "Native/Interfaces/IEntitlementsService.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/** Steam DLC ownership/listing via the global SteamApps() interface. */
	class FSteamEntitlements final : public IEntitlementsService
	{
	public:
		explicit FSteamEntitlements(FSteamPlatformCore& InCore) : Core(InCore) {}

		virtual void ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback) override;
		virtual void HasEntitlement(const FEntitlementDefinition& Definition,
		                            TFunction<void(const FHasEntitlementResult&)> Callback) override;

	private:
		FSteamPlatformCore& Core;
	};
}

#endif // USE_STEAMWORKS

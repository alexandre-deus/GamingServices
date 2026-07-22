#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IEntitlementsService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/** EOS entitlements capability (ECOM list + ownership check) over the shared platform core. */
	class FEOSEntitlements final : public IEntitlementsService
	{
	public:
		explicit FEOSEntitlements(FEOSPlatformCore& InCore) : Core(InCore) {}

		virtual void ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback) override;
		virtual void HasEntitlement(const FEntitlementDefinition& Definition,
		                            TFunction<void(const FHasEntitlementResult&)> Callback) override;

	private:
		FEOSPlatformCore& Core;
	};
}

#endif // USE_EOS

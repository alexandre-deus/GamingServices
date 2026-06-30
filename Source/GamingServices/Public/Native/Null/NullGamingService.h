#pragma once

#include "CoreMinimal.h"
#include "Native/IGamingService.h"

namespace GamingServices
{
	/**
	 * No-op backend used in the editor / when no platform SDK is configured.
	 *
	 * Unlike the legacy null service (which faked "success" for every call), this honest null overrides
	 * none of the capability accessors, so every Get*() returns the base's nullptr and GetCapabilities()
	 * is all-false. Callers correctly observe "no platform support" instead of silent no-ops.
	 */
	class GAMINGSERVICES_API FNullGamingService final : public IGamingService
	{
	public:
		virtual void Tick() override {}
		virtual bool IsInitialized() const override { return true; }
	};
}

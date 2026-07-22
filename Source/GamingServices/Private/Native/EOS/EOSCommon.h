#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "Native/EOS/EOSPlatformCore.h"

// The EOS SDK, included from one private place. The core header (EOSPlatformCore.h) is deliberately
// SDK-free and exposes its handles / ids as opaque void*, so every capability .cpp reaches the SDK
// through this umbrella instead.
#include "eos_sdk.h"
#include "eos_common.h"
#include "eos_auth.h"
#include "eos_achievements.h"
#include "eos_stats.h"
#include "eos_leaderboards.h"
#include "eos_connect.h"
#include "eos_logging.h"
#include "eos_playerdatastorage.h"
#include "eos_lobby.h"
#include "eos_ecom.h"
#include "eos_userinfo.h"

namespace GamingServices
{
	/**
	 * Cast the core's SDK-free void* product-user-id back to the typed EOS_ProductUserId that the EOS
	 * option structs require (void* does not implicitly convert to the handle type). This is the one
	 * cast shared by more than one capability, so it lives here rather than file-static in each .cpp —
	 * those copies collided when unity builds merged the translation units into one.
	 *
	 * inline, not an anonymous-namespace static: this header is included by capability files that don't
	 * all call the helper, and an unreferenced internal-linkage function trips C4505 under warnings-as-
	 * errors. An inline (external-linkage) definition drops unused copies silently and still resolves to
	 * a single definition, so it neither warns nor collides. File-local casts used by only one .cpp stay
	 * anonymous-namespace statics in that .cpp.
	 */
	inline EOS_ProductUserId ProductUserId(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_ProductUserId>(Core.GetProductUserId());
	}
}

#endif // USE_EOS

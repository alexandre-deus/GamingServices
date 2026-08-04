#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "Native/EOS/EOSPlatformCore.h"

// The single entry point every EOS capability .cpp includes for SDK access. It pulls in, in the one
// order that works: the SDK headers, the runtime symbol table built from them, and the redirects that
// rewrite EOS_* calls onto that table. Nothing may include an eos_*.h directly.
//
// The core header (EOSPlatformCore.h) is deliberately SDK-free and exposes its handles / ids as opaque
// void*, so every capability .cpp reaches the SDK through this umbrella instead.
#include "EOSDynamicApiRedirect.h"

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

	/**
	 * Render a ProductUserId as the string form the rest of the module passes around as a user id.
	 * Empty for a null or unrenderable id.
	 *
	 * Shared here for the same reason as the cast above: more than one capability needs it, and
	 * file-static copies collide once unity builds merge those translation units into one.
	 */
	inline FString PuidToString(EOS_ProductUserId Puid)
	{
		char Buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
		int32_t BufferLength = sizeof(Buffer);
		if (Puid && EOS_ProductUserId_ToString(Puid, Buffer, &BufferLength) == EOS_EResult::EOS_Success)
		{
			return UTF8_TO_TCHAR(Buffer);
		}
		return FString();
	}
}

#endif // GS_WITH_EOS

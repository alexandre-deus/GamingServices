#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"

namespace GamingServices
{
	/**
	 * Heap-allocated context threaded through EOS SDK callbacks as ClientData.
	 *
	 * Mirrors the legacy TEOSCallbackContext: it carries a typed result callback plus a back-pointer to
	 * the issuing object (here a capability class instead of the old fat service), and self-deletes once
	 * the callback runs. TOwner is the capability class that owns the in-flight request, so SDK callbacks
	 * can reach the platform core via Ctx->Service->Core.
	 */
	template <typename TResult, typename TOwner>
	struct TEOSCallbackContext
	{
		TOwner* Service;
		TFunction<void(const TResult&)> Callback;

		static TEOSCallbackContext* Create(TOwner* InService, TFunction<void(const TResult&)> InCallback)
		{
			TEOSCallbackContext* Ctx = new TEOSCallbackContext{};
			Ctx->Service = InService;
			Ctx->Callback = MoveTemp(InCallback);
			return Ctx;
		}

		static void Complete(TEOSCallbackContext* Ctx, const TResult& Result)
		{
			if (Ctx->Callback)
			{
				Ctx->Callback(Result);
			}
			delete Ctx;
		}
	};
}

#endif // GS_WITH_EOS

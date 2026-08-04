#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IExternalAuthService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/**
	 * EOS as the consumer of another platform's credential — the "...and run everything on EOS" half of
	 * a cross-backend login.
	 *
	 * Hands the credential to EOS Connect, which yields a ProductUserId without any Epic account being
	 * involved. Every EOS capability keys on that ProductUserId, so once this completes the backend is
	 * indistinguishable from one that authenticated through Epic.
	 */
	class FEOSExternalAuth final : public IExternalAuthConsumer
	{
	public:
		explicit FEOSExternalAuth(FEOSPlatformCore& InCore)
			: Core(InCore)
		{
		}

		virtual bool SupportsCredentialType(EExternalCredentialType Type) const override;

		virtual void LoginWithExternalCredential(const FExternalAuthCredential& Credential,
		                                         TFunction<void(const FGamingServiceResult&)> Callback) override;

	private:
		FEOSPlatformCore& Core;
	};
}

#endif // GS_WITH_EOS

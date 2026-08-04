#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IUserService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/**
	 * EOS user / identity capability over the shared platform core.
	 *
	 * Login runs the core's Auth -> Connect flow. EOS has no native avatar API, so GetAvatar* are
	 * nullptr stubs (FGamingServiceCapabilities::bAvatar is false for this backend) exactly as the
	 * legacy backend behaved.
	 */
	class FEOSUser final : public IUserService
	{
	public:
		explicit FEOSUser(FEOSPlatformCore& InCore) : Core(InCore) {}

		virtual void Login(const FGamingServiceLoginParams& Params,
		                   TFunction<void(const FGamingServiceResult&)> Callback) override;

		virtual bool IsLoggedIn() const override;
		virtual bool NeedsLogin() const override;
		virtual FString GetUserId() const override;
		virtual FString GetDisplayName() const override;

		virtual void ResolveDisplayName(const FString& UserId,
		                                TFunction<void(const FResolveDisplayNameResult&)> Callback) override;

		virtual UTexture2D* GetAvatar() const override;
		virtual UTexture2D* GetAvatarByUserId(const FString& UserId) const override;

	private:
		FEOSPlatformCore& Core;
	};
}

#endif // GS_WITH_EOS

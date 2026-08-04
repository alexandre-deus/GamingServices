#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IUserService.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/**
	 * Steam local-user identity + avatar.
	 *
	 * Identity comes from FSteamPlatformCore. The avatar cache and its Steam CCallbacks
	 * (AvatarImageLoaded_t / PersonaStateChange_t) live in a private FImpl (pimpl) so this header
	 * stays free of the Steam SDK. Avatar-ready fires this object's own OnAvatarReady sink.
	 */
	class FSteamUser final : public IUserService
	{
	public:
		explicit FSteamUser(FSteamPlatformCore& InCore);

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

		/** Eagerly fetch a member's user info + avatar so Steam delivers AvatarImageLoaded_t for them. */
		void EnsureAvatarForMember(uint64 SteamID64);

	private:
		struct FImpl;

		FSteamPlatformCore& Core;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_STEAM

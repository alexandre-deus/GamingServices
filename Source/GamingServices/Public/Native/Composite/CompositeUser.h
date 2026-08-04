#pragma once

#include "CoreMinimal.h"
#include "Native/Interfaces/IUserService.h"

class IGamingService;

namespace GamingServices
{
	/**
	 * The user capability of a multi-backend arrangement: one backend proves who the player is, another
	 * owns the session.
	 *
	 * Login runs the bridge — ask the identity backend for a credential, hand it to the session
	 * backend's external-auth consumer — which is what makes "sign in with Steam, play on EOS" work
	 * without the player ever seeing an Epic login.
	 *
	 * Identity afterwards is deliberately split:
	 *   - GetUserId() is the SESSION backend's id (an EOS ProductUserId), because that is what
	 *     matchmaking, P2P, stats and every other capability key on. Handing out the Steam id here
	 *     would break every one of them.
	 *   - GetDisplayName() / GetAvatar() prefer the IDENTITY backend, so the player sees their Steam
	 *     persona and avatar rather than a blank EOS profile.
	 *
	 * Resolving OTHER players is asymmetric and cannot be otherwise: remote members arrive as session
	 * ids, and the identity backend has no mapping from those to its own users. Name resolution goes to
	 * the session backend (EOS carries the display name supplied at login, so Steam personas do come
	 * back); avatars for remote players resolve only when the session backend has them, which is why
	 * GetAvatarByUserId falls back rather than pretending.
	 */
	class GAMINGSERVICES_API FCompositeUser final : public IUserService
	{
	public:
		/**
		 * @param InSession   backend that owns the session and its identity (the primary).
		 * @param InIdentity  backend that authenticates the player, or null for no bridge.
		 * @param bInAllowFallback  fall back to the session backend's own login when the bridge is
		 *                          unavailable or the credential request fails.
		 */
		FCompositeUser(IGamingService& InSession, IGamingService* InIdentity, bool bInAllowFallback);
		virtual ~FCompositeUser() override;

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
		IUserService* GetSessionUser() const;
		IUserService* GetIdentityUser() const;

		/** Runs the session backend's own login. Used when there is no bridge, or as the fallback. */
		void LoginWithSessionBackend(const FGamingServiceLoginParams& Params,
		                             TFunction<void(const FGamingServiceResult&)> Callback);

		IGamingService& Session;
		IGamingService* Identity = nullptr;
		bool bAllowFallback = true;

		/** Identity-side details captured at login, so they survive independently of that backend's state. */
		FString IdentityDisplayName;
		FString IdentityNativeUserId;
	};
}

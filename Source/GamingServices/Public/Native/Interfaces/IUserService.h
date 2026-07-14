#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "DataTypes/LoginTypes.h"
#include "DataTypes/UserTypes.h"
#include "Native/GamingCapability.h"

class UTexture2D;

/**
 * Local-user identity + authentication capability: login state, user id / display name, and
 * (where the platform supports it) avatar textures.
 *
 * Avatar support is a sub-capability: GetAvatar* may return nullptr on backends without a native
 * avatar API (EOS). FGamingServiceCapabilities::bAvatar reflects this.
 */
class GAMINGSERVICES_API IUserService
{
public:
	virtual ~IUserService() = default;

	virtual void Login(const FGamingServiceLoginParams& Params,
	                   TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	virtual bool IsLoggedIn() const = 0;
	virtual bool NeedsLogin() const = 0;
	virtual FString GetUserId() const = 0;
	virtual FString GetDisplayName() const = 0;

	/**
	 * Asynchronously resolve any user id to a display name. This is the counterpart to the id-only
	 * member events fired by IMatchmakingService (OnSessionUserJoined / OnSessionUserLeft carry only
	 * the id): pass that id here to get the name when you want it.
	 *
	 * The callback always fires exactly once with the same UserId that was passed in. On failure (or
	 * a platform that cannot resolve the id) DisplayName falls back to the UserId string, so callers
	 * always receive a non-empty, displayable value.
	 *
	 * Backends: Steam resolves via ISteamFriends (immediate when cached, otherwise after Steam
	 * downloads the persona). EOS resolves via the Connect id-mapping + product-user info cache.
	 */
	virtual void ResolveDisplayName(const FString& UserId,
	                                TFunction<void(const FResolveDisplayNameResult&)> Callback) = 0;

	virtual UTexture2D* GetAvatar() const = 0;
	virtual UTexture2D* GetAvatarByUserId(const FString& UserId) const = 0;

	// Fires after the platform finishes asynchronously fetching an avatar texture for UserId.
	TFunction<void(const FString& UserId)> OnAvatarReady;
};

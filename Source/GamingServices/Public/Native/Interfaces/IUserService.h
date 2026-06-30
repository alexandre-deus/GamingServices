#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "DataTypes/LoginTypes.h"
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

	virtual UTexture2D* GetAvatar() const = 0;
	virtual UTexture2D* GetAvatarByUserId(const FString& UserId) const = 0;

	// Fires after the platform finishes asynchronously fetching an avatar texture for UserId.
	TFunction<void(const FString& UserId)> OnAvatarReady;
};

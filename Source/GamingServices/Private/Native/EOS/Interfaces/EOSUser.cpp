#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSUser.h"
#include "Native/EOS/EOSPlatformCore.h"

namespace GamingServices
{
	void FEOSUser::Login(const FGamingServiceLoginParams& Params,
	                     TFunction<void(const FGamingServiceResult&)> Callback)
	{
		Core.Login(Params, MoveTemp(Callback));
	}

	bool FEOSUser::IsLoggedIn() const
	{
		return Core.IsLoggedIn();
	}

	bool FEOSUser::NeedsLogin() const
	{
		return Core.NeedsLogin();
	}

	FString FEOSUser::GetUserId() const
	{
		return Core.GetUserId();
	}

	FString FEOSUser::GetDisplayName() const
	{
		return Core.GetDisplayName();
	}

	// Avatars are unavailable on EOS through any Epic-provided path. The EOS C++ SDK has no avatar
	// API at all, and UE's own OnlineServices EOS backend inherits FUserInfoCommon::GetUserAvatar,
	// which returns Errors::NotImplemented() (Engine/Plugins/Online/OnlineServices). Epic account
	// avatars only exist behind the Epic Games web/Account REST API (an avatar-URL fetch + async
	// texture download), which needs a separate HTTP path and is out of scope for the SDK backend.
	// Wire a source here (Epic web API or a game backend) if EOS avatars are ever needed.
	UTexture2D* FEOSUser::GetAvatar() const
	{
		return nullptr;
	}

	UTexture2D* FEOSUser::GetAvatarByUserId(const FString& UserId) const
	{
		return nullptr;
	}
}

#endif // USE_EOS

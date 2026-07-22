#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSUser.h"
#include "EOSCommon.h"

namespace GamingServices
{
	void FEOSUser::Login(const FGamingServiceLoginParams& Params,
	                     TFunction<void(const FGamingServiceResult&)> Callback)
	{
		Core.Login(Params, MoveTemp(Callback));
	}

	void FEOSUser::ResolveDisplayName(const FString& UserId,
	                                  TFunction<void(const FResolveDisplayNameResult&)> Callback)
	{
		const auto Complete = [Callback, UserId](const FString& Name)
		{
			// A genuine name is success; an empty name falls back to the id (still non-empty).
			Callback(Name.IsEmpty()
				         ? FResolveDisplayNameResult::Fallback(UserId)
				         : FResolveDisplayNameResult::Resolved(UserId, Name));
		};

		EOS_HConnect ConnectHandle = static_cast<EOS_HConnect>(Core.GetConnectHandle());
		const EOS_ProductUserId LocalUser = static_cast<EOS_ProductUserId>(Core.GetProductUserId());
		const EOS_ProductUserId TargetUser = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*UserId));

		if (!ConnectHandle || !LocalUser || !EOS_ProductUserId_IsValid(TargetUser))
		{
			Complete(FString());
			return;
		}

		// Resolving our own id needs no round-trip.
		if (TargetUser == LocalUser && !Core.GetDisplayName().IsEmpty())
		{
			Complete(Core.GetDisplayName());
			return;
		}

		struct FResolveCtx
		{
			FEOSUser* Service;
			EOS_ProductUserId TargetUser;
			TFunction<void(const FString&)> Complete;
		};
		auto* Ctx = new FResolveCtx{this, TargetUser, Complete};

		EOS_ProductUserId Ids[1] = {TargetUser};
		EOS_Connect_QueryProductUserIdMappingsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST;
		QueryOptions.LocalUserId = LocalUser;
		QueryOptions.ProductUserIds = Ids;
		QueryOptions.ProductUserIdCount = 1;

		EOS_Connect_QueryProductUserIdMappings(
			ConnectHandle,
			&QueryOptions,
			Ctx,
			[](const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FResolveCtx> LocalCtx(static_cast<FResolveCtx*>(Data->ClientData));

				FString Resolved;
				if (Data->ResultCode == EOS_EResult::EOS_Success)
				{
					EOS_HConnect Handle = static_cast<EOS_HConnect>(LocalCtx->Service->Core.GetConnectHandle());

					EOS_Connect_CopyProductUserInfoOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_CONNECT_COPYPRODUCTUSERINFO_API_LATEST;
					CopyOptions.TargetUserId = LocalCtx->TargetUser;

					EOS_Connect_ExternalAccountInfo* Info = nullptr;
					if (EOS_Connect_CopyProductUserInfo(Handle, &CopyOptions, &Info) == EOS_EResult::EOS_Success && Info)
					{
						if (Info->DisplayName)
						{
							Resolved = UTF8_TO_TCHAR(Info->DisplayName);
						}
						EOS_Connect_ExternalAccountInfo_Release(Info);
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: ResolveDisplayName query failed: %d"),
					       (int32)Data->ResultCode);
				}

				LocalCtx->Complete(Resolved);
			});
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

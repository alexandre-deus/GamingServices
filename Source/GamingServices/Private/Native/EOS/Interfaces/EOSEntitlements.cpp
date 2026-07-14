#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSEntitlements.h"
#include "Native/EOS/EOSPlatformCore.h"
#include "EOSCallbackContext.h"

#include <string>

namespace GamingServices
{
	using FEntitlementsListCallbackCtx = TEOSCallbackContext<FEntitlementsListResult, FEOSEntitlements>;
	using FHasEntitlementCallbackCtx = TEOSCallbackContext<FHasEntitlementResult, FEOSEntitlements>;

	// Cast the core's opaque accessors back to their EOS_* types in this .cpp so the core header stays SDK-free.
	static EOS_HEcom EcomHandle(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_HEcom>(Core.GetEcomHandle());
	}

	static EOS_EpicAccountId EpicAccountId(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_EpicAccountId>(Core.GetEpicAccountId());
	}

	void FEOSEntitlements::ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetEcomHandle(),
		       TEXT("EOSGamingService: ListEntitlements called when service not ready"));

		EOS_Ecom_QueryEntitlementsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_ECOM_QUERYENTITLEMENTS_API_LATEST;
		QueryOptions.LocalUserId = EpicAccountId(Core);
		QueryOptions.EntitlementNameCount = 0;
		QueryOptions.EntitlementNames = nullptr;
		QueryOptions.bIncludeRedeemed = EOS_TRUE;

		auto* Ctx = FEntitlementsListCallbackCtx::Create(this, MoveTemp(Callback));

		EOS_Ecom_QueryEntitlements(
			EcomHandle(Core),
			&QueryOptions,
			Ctx,
			[](const EOS_Ecom_QueryEntitlementsCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FEntitlementsListCallbackCtx*>(Data->ClientData);
				FEOSEntitlements* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);

				FEntitlementsListResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!Result.bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: QueryEntitlements failed: %d"),
					       (int32)Data->ResultCode);
					FEntitlementsListCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				EOS_Ecom_GetEntitlementsCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_ECOM_GETENTITLEMENTSCOUNT_API_LATEST;
				CountOptions.LocalUserId = EpicAccountId(Self->Core);
				uint32_t Count = EOS_Ecom_GetEntitlementsCount(EcomHandle(Self->Core), &CountOptions);

				TArray<FEntitlement> OutEntitlements;
				for (uint32_t Index = 0; Index < Count; ++Index)
				{
					EOS_Ecom_CopyEntitlementByIndexOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_ECOM_COPYENTITLEMENTBYINDEX_API_LATEST;
					CopyOptions.LocalUserId = EpicAccountId(Self->Core);
					CopyOptions.EntitlementIndex = Index;

					EOS_Ecom_Entitlement* Entitlement = nullptr;
					if (EOS_Ecom_CopyEntitlementByIndex(EcomHandle(Self->Core), &CopyOptions, &Entitlement) ==
						EOS_EResult::EOS_Success && Entitlement)
					{
						if (Entitlement->bRedeemed == EOS_TRUE)
						{
							FEntitlement E;
							E.Id = UTF8_TO_TCHAR(Entitlement->EntitlementId ? Entitlement->EntitlementId : "");
							E.DisplayName = UTF8_TO_TCHAR(Entitlement->EntitlementName ? Entitlement->EntitlementName : "");
							E.Description = TEXT("");
							OutEntitlements.Add(E);
						}
						EOS_Ecom_Entitlement_Release(Entitlement);
					}
				}

				Result.Entitlements = OutEntitlements;
				FEntitlementsListCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void FEOSEntitlements::HasEntitlement(const FEntitlementDefinition& Definition,
	                                      TFunction<void(const FHasEntitlementResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetEcomHandle(),
		       TEXT("EOSGamingService: HasEntitlement called when service not ready"));

		std::string EntitlementNameUtf8 = TCHAR_TO_UTF8(*Definition.EOSEntitlementName);
		const char* EntitlementNames[1];
		EntitlementNames[0] = EntitlementNameUtf8.c_str();

		EOS_Ecom_QueryEntitlementsOptions QueryOptions = {};
		QueryOptions.ApiVersion = EOS_ECOM_QUERYENTITLEMENTS_API_LATEST;
		QueryOptions.LocalUserId = EpicAccountId(Core);
		QueryOptions.EntitlementNameCount = 1;
		QueryOptions.EntitlementNames = EntitlementNames;
		QueryOptions.bIncludeRedeemed = EOS_TRUE;

		struct FHasEntitlementCtx : FHasEntitlementCallbackCtx
		{
			FString EntitlementName;
		};

		auto* Ctx = new FHasEntitlementCtx{};
		Ctx->Service = this;
		Ctx->Callback = MoveTemp(Callback);
		Ctx->EntitlementName = Definition.EOSEntitlementName;

		EOS_Ecom_QueryEntitlements(
			EcomHandle(Core),
			&QueryOptions,
			Ctx,
			[](const EOS_Ecom_QueryEntitlementsCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FHasEntitlementCtx*>(Data->ClientData);
				FEOSEntitlements* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);

				FHasEntitlementResult Result;
				Result.EntitlementId = LocalCtx->EntitlementName;

				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: HasEntitlement query failed: %d"),
					       (int32)Data->ResultCode);
					Result.bSuccess = false;
					Result.bHasEntitlement = false;
					FHasEntitlementCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				// Must outlive the EOS_Ecom_GetEntitlementsByNameCount call below; assigning
				// TCHAR_TO_UTF8() directly leaves a dangling pointer.
				const std::string EntitlementNameUtf8 = TCHAR_TO_UTF8(*LocalCtx->EntitlementName);

				EOS_Ecom_GetEntitlementsByNameCountOptions CountOptions = {};
				CountOptions.ApiVersion = EOS_ECOM_GETENTITLEMENTSBYNAMECOUNT_API_LATEST;
				CountOptions.LocalUserId = EpicAccountId(Self->Core);
				CountOptions.EntitlementName = EntitlementNameUtf8.c_str();

				uint32_t Count = EOS_Ecom_GetEntitlementsByNameCount(EcomHandle(Self->Core), &CountOptions);
				Result.bSuccess = true;
				Result.bHasEntitlement = (Count > 0);

				FHasEntitlementCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}
}

#endif // USE_EOS

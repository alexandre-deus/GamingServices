#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamEntitlements.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	void FSteamEntitlements::ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback)
	{
		ISteamApps* SteamApps = ::SteamApps();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamApps,
			   TEXT("SteamworksGamingService: ListEntitlements called when service not ready"));

		FEntitlementsListResult Result;
		Result.bSuccess = true;

		int32 DLCCount = SteamApps->GetDLCCount();
		for (int32 Index = 0; Index < DLCCount; ++Index)
		{
			AppId_t AppId = 0;
			bool bAvailable = false;
			char Name[256] = {0};

			if (SteamApps->BGetDLCDataByIndex(Index, &AppId, &bAvailable, Name, sizeof(Name)))
			{
				FEntitlement E;
				E.Id = FString::Printf(TEXT("%u"), (uint32)AppId);
				E.DisplayName = UTF8_TO_TCHAR(Name);
				E.Description = TEXT("");
				Result.Entitlements.Add(E);
			}
		}

		if (Callback)
		{
			Callback(Result);
		}
	}

	void FSteamEntitlements::HasEntitlement(const FEntitlementDefinition& Definition,
	                                        TFunction<void(const FHasEntitlementResult&)> Callback)
	{
		ISteamApps* SteamApps = ::SteamApps();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamApps,
			   TEXT("SteamworksGamingService: HasEntitlement called when service not ready"));

		AppId_t AppId = (AppId_t)Definition.SteamAppId;
		FHasEntitlementResult Result;
		Result.EntitlementId = FString::Printf(TEXT("%u"), (uint32)AppId);

		if (AppId == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: HasEntitlement called with AppId 0"));
			Result.bSuccess = false;
			Result.bHasEntitlement = false;
		}
		else
		{
			bool bOwned = SteamApps->BIsSubscribedApp(AppId) || SteamApps->BIsDlcInstalled(AppId);
			Result.bSuccess = true;
			Result.bHasEntitlement = bOwned;
		}

		if (Callback)
		{
			Callback(Result);
		}
	}
}

#endif // GS_WITH_STEAM

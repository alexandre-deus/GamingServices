#include "GamingServices.h"
#include "Services/EOSGamingService.h"
#include "Services/SteamworksGamingService.h"
#include "Services/NullGamingService.h"

IMPLEMENT_MODULE(FGamingServicesModule, GamingServices)

void FGamingServicesModule::StartupModule()
{
#ifdef USE_EOS
	Service = MakeUnique<FEOSGamingService>();
#elif defined(USE_STEAMWORKS)
	Service = MakeUnique<FSteamworksGamingService>();
#else
	Service = MakeUnique<FNullGamingService>();
#endif
	Service->InitializePlatform();
}

void FGamingServicesModule::ShutdownModule()
{
	if (Service)
	{
		Service->DestroyPlatform();
		Service.Reset();
	}
}

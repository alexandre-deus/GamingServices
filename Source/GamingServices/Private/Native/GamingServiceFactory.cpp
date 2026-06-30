#include "Native/GamingServiceFactory.h"
#include "Native/Null/NullGamingService.h"

#if defined(USE_EOS)
#include "Native/EOS/EOSGamingService.h"
#elif defined(USE_STEAMWORKS)
#include "Native/Steam/SteamGamingService.h"
#endif

namespace GamingServices
{
	TUniquePtr<IGamingService> CreateGamingService()
	{
#if defined(USE_EOS)
		return MakeUnique<FEOSGamingService>();
#elif defined(USE_STEAMWORKS)
		return MakeUnique<FSteamGamingService>();
#else
		return MakeUnique<FNullGamingService>();
#endif
	}
}

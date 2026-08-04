#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"

namespace GamingServices
{
	/**
	 * Whether the Steamworks shared library was found and its entry points resolved.
	 *
	 * Steamworks is not linked. Its C++ interfaces (ISteamUser, ISteamMatchmaking, ...) are pure-virtual
	 * and dispatch through vtables obtained from the library, so the only symbols that must be bound are
	 * the handful of global C entry points the SDK headers and callback templates reference — supplied
	 * as forwarders in SteamDynamicApi.cpp.
	 *
	 * The first call triggers the load. When the library is absent every forwarder degrades safely
	 * (SteamAPI_Init fails, the interface accessors yield null), so calling Steam without checking is
	 * not fatal — but the backend uses this to report itself unavailable up front.
	 */
	bool IsSteamApiAvailable();
}

#endif // GS_WITH_STEAM

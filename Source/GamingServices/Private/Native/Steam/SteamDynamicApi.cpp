#ifdef GS_WITH_STEAM

#include "SteamDynamicApi.h"

#include "GamingSdkLibrary.h"

#include "steam/steam_api.h"

#ifndef GS_STEAM_LIBRARY_NAME
#define GS_STEAM_LIBRARY_NAME "steam_api64.dll"
#endif

/**
 * Runtime binding for Steamworks.
 *
 * The SDK is not linked: GamingServices.Build.cs adds no import library and defines STEAM_API_NODLL, so
 * Steam's headers declare their global C entry points as plain extern "C" instead of dllimport. This
 * file supplies those entry points, forwarding each to a symbol resolved from the Steam shared library
 * out of the common SDK folder.
 *
 * Only the globals are needed. Everything the capabilities actually call — ISteamUser, ISteamFriends,
 * ISteamMatchmaking, ISteamNetworkingSockets and friends — is a pure-virtual interface whose vtable
 * comes from the library itself, reached through the inline accessors in the SDK headers. Those
 * accessors bottom out in SteamInternal_ContextInit / SteamInternal_FindOrCreateUserInterface, which
 * are among the forwarders below, so binding this short list covers the whole C++ surface.
 *
 * When the library is missing, each forwarder returns a benign value rather than crashing: Init reports
 * failure, the context accessors hand back a slot holding null so SteamUser() and friends evaluate to
 * nullptr, and the callback registration calls become no-ops. The existing null checks throughout the
 * Steam capabilities then take over.
 */
namespace GamingServices
{
	namespace
	{
		struct FSteamApi
		{
			ESteamAPIInitResult (S_CALLTYPE* SteamInternal_SteamAPI_Init)(const char*, SteamErrMsg*) = nullptr;
			void  (S_CALLTYPE* SteamAPI_Shutdown)() = nullptr;
			bool  (S_CALLTYPE* SteamAPI_RestartAppIfNecessary)(uint32) = nullptr;
			bool  (S_CALLTYPE* SteamAPI_IsSteamRunning)() = nullptr;
			void  (S_CALLTYPE* SteamAPI_RunCallbacks)() = nullptr;
			void  (S_CALLTYPE* SteamAPI_ReleaseCurrentThreadMemory)() = nullptr;
			HSteamPipe (S_CALLTYPE* SteamAPI_GetHSteamPipe)() = nullptr;
			HSteamUser (S_CALLTYPE* SteamAPI_GetHSteamUser)() = nullptr;
			void* (S_CALLTYPE* SteamInternal_ContextInit)(void*) = nullptr;
			void* (S_CALLTYPE* SteamInternal_CreateInterface)(const char*) = nullptr;
			void* (S_CALLTYPE* SteamInternal_FindOrCreateUserInterface)(HSteamUser, const char*) = nullptr;
			void  (S_CALLTYPE* SteamAPI_RegisterCallback)(CCallbackBase*, int) = nullptr;
			void  (S_CALLTYPE* SteamAPI_UnregisterCallback)(CCallbackBase*) = nullptr;
			void  (S_CALLTYPE* SteamAPI_RegisterCallResult)(CCallbackBase*, SteamAPICall_t) = nullptr;
			void  (S_CALLTYPE* SteamAPI_UnregisterCallResult)(CCallbackBase*, SteamAPICall_t) = nullptr;
			HSteamUser (S_CALLTYPE* SteamGameServer_GetHSteamUser)() = nullptr;
			HSteamPipe (S_CALLTYPE* SteamGameServer_GetHSteamPipe)() = nullptr;
			void* (S_CALLTYPE* SteamInternal_FindOrCreateGameServerInterface)(HSteamUser, const char*) = nullptr;

			bool bAvailable = false;

			FSteamApi()
			{
				static FGamingSdkLibrary Library(TEXT(GS_STEAM_LIBRARY_NAME));
				if (!Library.Load())
				{
					return;
				}

				TArray<FString> MissingSymbols;

#define GS_STEAM_BIND(Symbol) \
	Symbol = reinterpret_cast<decltype(Symbol)>(Library.GetExport(TEXT(#Symbol))); \
	if (!Symbol) { MissingSymbols.Add(TEXT(#Symbol)); }

				GS_STEAM_BIND(SteamInternal_SteamAPI_Init)
				GS_STEAM_BIND(SteamAPI_Shutdown)
				GS_STEAM_BIND(SteamAPI_RestartAppIfNecessary)
				GS_STEAM_BIND(SteamAPI_IsSteamRunning)
				GS_STEAM_BIND(SteamAPI_RunCallbacks)
				GS_STEAM_BIND(SteamAPI_ReleaseCurrentThreadMemory)
				GS_STEAM_BIND(SteamAPI_GetHSteamPipe)
				GS_STEAM_BIND(SteamAPI_GetHSteamUser)
				GS_STEAM_BIND(SteamInternal_ContextInit)
				GS_STEAM_BIND(SteamInternal_CreateInterface)
				GS_STEAM_BIND(SteamInternal_FindOrCreateUserInterface)
				GS_STEAM_BIND(SteamAPI_RegisterCallback)
				GS_STEAM_BIND(SteamAPI_UnregisterCallback)
				GS_STEAM_BIND(SteamAPI_RegisterCallResult)
				GS_STEAM_BIND(SteamAPI_UnregisterCallResult)
				GS_STEAM_BIND(SteamGameServer_GetHSteamUser)
				GS_STEAM_BIND(SteamGameServer_GetHSteamPipe)
				GS_STEAM_BIND(SteamInternal_FindOrCreateGameServerInterface)

#undef GS_STEAM_BIND

				if (MissingSymbols.Num() > 0)
				{
					UE_LOG(LogTemp, Error,
					       TEXT("GamingServices: Steam library '%s' is missing expected entry point(s): %s. "
						       "Steamworks will be unavailable."),
					       *Library.GetLibraryName(), *FString::Join(MissingSymbols, TEXT(", ")));
					return;
				}

				bAvailable = true;
			}
		};

		/**
		 * Resolved once, on first use. A function-local static gives thread-safe one-time construction,
		 * which matters because the interface accessors below are reachable from any thread that touches
		 * a Steam interface.
		 */
		const FSteamApi& SteamApi()
		{
			static const FSteamApi Api;
			return Api;
		}

		/**
		 * Stand-in for the interface-pointer slot SteamInternal_ContextInit normally returns. The SDK's
		 * accessor macro dereferences the result unconditionally, so a null return would crash; pointing
		 * at a permanently-null slot instead makes SteamUser(), SteamFriends(), ... evaluate to nullptr,
		 * which every Steam capability here already checks for.
		 */
		void* GNullInterfaceSlot = nullptr;
	}

	bool IsSteamApiAvailable()
	{
		return SteamApi().bAvailable;
	}
}

// ---------------------------------------------------------------------------------------------------
// Steamworks global entry points. Signatures must match the SDK headers exactly.
// ---------------------------------------------------------------------------------------------------

using namespace GamingServices;

S_API ESteamAPIInitResult S_CALLTYPE SteamInternal_SteamAPI_Init(const char* pszInternalCheckInterfaceVersions, SteamErrMsg* pOutErrMsg)
{
	if (const auto Fn = SteamApi().SteamInternal_SteamAPI_Init)
	{
		return Fn(pszInternalCheckInterfaceVersions, pOutErrMsg);
	}
	if (pOutErrMsg)
	{
		FCStringAnsi::Strncpy(*pOutErrMsg, "Steamworks library not present", k_cchMaxSteamErrMsg);
	}
	return k_ESteamAPIInitResult_FailedGeneric;
}

S_API void S_CALLTYPE SteamAPI_Shutdown()
{
	if (const auto Fn = SteamApi().SteamAPI_Shutdown)
	{
		Fn();
	}
}

S_API bool S_CALLTYPE SteamAPI_RestartAppIfNecessary(uint32 unOwnAppID)
{
	const auto Fn = SteamApi().SteamAPI_RestartAppIfNecessary;
	return Fn ? Fn(unOwnAppID) : false;
}

S_API bool S_CALLTYPE SteamAPI_IsSteamRunning()
{
	const auto Fn = SteamApi().SteamAPI_IsSteamRunning;
	return Fn ? Fn() : false;
}

S_API void S_CALLTYPE SteamAPI_RunCallbacks()
{
	if (const auto Fn = SteamApi().SteamAPI_RunCallbacks)
	{
		Fn();
	}
}

S_API void S_CALLTYPE SteamAPI_ReleaseCurrentThreadMemory()
{
	if (const auto Fn = SteamApi().SteamAPI_ReleaseCurrentThreadMemory)
	{
		Fn();
	}
}

S_API HSteamPipe S_CALLTYPE SteamAPI_GetHSteamPipe()
{
	const auto Fn = SteamApi().SteamAPI_GetHSteamPipe;
	return Fn ? Fn() : 0;
}

S_API HSteamUser S_CALLTYPE SteamAPI_GetHSteamUser()
{
	const auto Fn = SteamApi().SteamAPI_GetHSteamUser;
	return Fn ? Fn() : 0;
}

S_API void* S_CALLTYPE SteamInternal_ContextInit(void* pContextInitData)
{
	const auto Fn = SteamApi().SteamInternal_ContextInit;
	return Fn ? Fn(pContextInitData) : &GNullInterfaceSlot;
}

S_API void* S_CALLTYPE SteamInternal_CreateInterface(const char* ver)
{
	const auto Fn = SteamApi().SteamInternal_CreateInterface;
	return Fn ? Fn(ver) : nullptr;
}

S_API void* S_CALLTYPE SteamInternal_FindOrCreateUserInterface(HSteamUser hSteamUser, const char* pszVersion)
{
	const auto Fn = SteamApi().SteamInternal_FindOrCreateUserInterface;
	return Fn ? Fn(hSteamUser, pszVersion) : nullptr;
}

S_API void S_CALLTYPE SteamAPI_RegisterCallback(CCallbackBase* pCallback, int iCallback)
{
	if (const auto Fn = SteamApi().SteamAPI_RegisterCallback)
	{
		Fn(pCallback, iCallback);
	}
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallback(CCallbackBase* pCallback)
{
	if (const auto Fn = SteamApi().SteamAPI_UnregisterCallback)
	{
		Fn(pCallback);
	}
}

S_API void S_CALLTYPE SteamAPI_RegisterCallResult(CCallbackBase* pCallback, SteamAPICall_t hAPICall)
{
	if (const auto Fn = SteamApi().SteamAPI_RegisterCallResult)
	{
		Fn(pCallback, hAPICall);
	}
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallResult(CCallbackBase* pCallback, SteamAPICall_t hAPICall)
{
	if (const auto Fn = SteamApi().SteamAPI_UnregisterCallResult)
	{
		Fn(pCallback, hAPICall);
	}
}

S_API HSteamUser S_CALLTYPE SteamGameServer_GetHSteamUser()
{
	const auto Fn = SteamApi().SteamGameServer_GetHSteamUser;
	return Fn ? Fn() : 0;
}

S_API HSteamPipe S_CALLTYPE SteamGameServer_GetHSteamPipe()
{
	const auto Fn = SteamApi().SteamGameServer_GetHSteamPipe;
	return Fn ? Fn() : 0;
}

S_API void* S_CALLTYPE SteamInternal_FindOrCreateGameServerInterface(HSteamUser hSteamUser, const char* pszVersion)
{
	const auto Fn = SteamApi().SteamInternal_FindOrCreateGameServerInterface;
	return Fn ? Fn(hSteamUser, pszVersion) : nullptr;
}

#endif // GS_WITH_STEAM

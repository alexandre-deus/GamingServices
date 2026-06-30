#pragma once

#ifdef USE_STEAMWORKS

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Templates/UniquePtr.h"

namespace GamingServices
{
	struct FSteamCallResultManager;

	/**
	 * Owns the shared, capability-agnostic Steam state for the decomposed backend: lifecycle,
	 * login/identity, and the CallResults pump. Steam interfaces are obtained by the capability
	 * classes themselves via the global accessors (SteamUserStats(), SteamMatchmaking(), ...);
	 * this core no longer caches ISteam* pointers, nor does it hold any lobby/avatar state.
	 *
	 * The header is deliberately free of steam/steam_api.h: the owned FSteamCallResultManager is
	 * forward-declared and held by TUniquePtr so all SDK usage stays in the .cpp.
	 */
	class FSteamPlatformCore
	{
	public:
		FSteamPlatformCore();
		~FSteamPlatformCore();

		// ---- Lifecycle ----
		// Reads the configured AppId, honours SteamAPI_RestartAppIfNecessary, then InitializeSteamworks().
		void InitializePlatform();
		void DestroyPlatform();
		void Tick();

		void InitializeSteamworks();
		void ShutdownSteamworks();

		// ---- State queries ----
		bool IsInitialized() const { return bIsInitialized; }
		bool IsConnected() const { return bIsInitialized; }
		bool IsLoggedIn() const { return bIsLoggedIn; }
		bool NeedsLogin() const { return false; }
		const FString& GetUserId() const { return UserId; }
		const FString& GetDisplayName() const { return DisplayName; }

		bool IsSteamRunning() const;
		bool IsSteamOverlayEnabled() const;

		// ---- CallResults pump (owned; capabilities register async ops here) ----
		FSteamCallResultManager& GetCallResults() { return *CallResults; }

	private:
		bool bIsInitialized = false;
		bool bIsLoggedIn = false;
		FString UserId;
		FString DisplayName;

		TUniquePtr<FSteamCallResultManager> CallResults;

		FCriticalSection CallbackCriticalSection;
	};
}

#endif // USE_STEAMWORKS

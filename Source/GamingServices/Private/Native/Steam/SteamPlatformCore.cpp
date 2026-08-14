#ifdef GS_WITH_STEAM

#include "Native/Steam/SteamPlatformCore.h"
#include "SteamCallResultManager.h"
#include "SteamDynamicApi.h"

#include "HAL/PlatformMisc.h"
#include "Misc/ConfigCacheIni.h"

namespace GamingServices
{
	FSteamPlatformCore::FSteamPlatformCore()
		: CallResults(MakeUnique<FSteamCallResultManager>())
	{
	}

	FSteamPlatformCore::~FSteamPlatformCore()
	{
		ShutdownSteamworks();
	}

	void FSteamPlatformCore::InitializePlatform()
	{
		// The SDK is not linked — check the library resolved before the first Steam call. A build or
		// machine without it simply leaves this backend uninitialized.
		if (!IsSteamApiAvailable())
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steam library unavailable; Steam backend disabled"));
			return;
		}

		int32 AppIdInt = 0;
		if (!GConfig->GetInt(TEXT("GamingServices.Steamworks"), TEXT("AppId"), AppIdInt, GGameIni) || AppIdInt <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Missing or invalid [GamingServices.Steamworks] AppId in GameIni; skipping Steam init"));
			return;
		}

		const uint32 AppId = static_cast<uint32>(AppIdInt);
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: AppId=%u from config"), AppId);

		// The relaunch handshake only makes sense for a shipped build started outside Steam. Editor
		// builds — including standalone launched from the editor — run out of the engine's binaries
		// directory, where Steam finds no steam_appid.txt, so it would ask us to restart as the
		// configured AppId every time. Steam also requires the process to end immediately here; the
		// cooperative RequestExit(false) unwound the engine mid-module-load and asserted on missing
		// generated code instead of exiting.
#if !WITH_EDITOR
		if (SteamAPI_RestartAppIfNecessary(AppId))
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: SteamAPI_RestartAppIfNecessary requested relaunch via Steam; exiting"));
			FPlatformMisc::RequestExit(true);
			return;
		}
#else
		// Same working-directory problem seen from the other side: with no steam_appid.txt to find,
		// SteamAPI_InitEx fails outright with "No appID found". The SDK reads the id from the
		// environment too, which keeps the answer on the config value above instead of depending on a
		// stray file landing next to whichever executable the editor happened to launch.
		const FString AppIdStr = FString::FromInt(AppIdInt);
		FPlatformMisc::SetEnvironmentVar(TEXT("SteamAppId"), *AppIdStr);
		FPlatformMisc::SetEnvironmentVar(TEXT("SteamGameId"), *AppIdStr);
#endif

		InitializeSteamworks();
	}

	void FSteamPlatformCore::DestroyPlatform()
	{
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Starting shutdown..."));

		bIsLoggedIn = false;
		UserId.Empty();
		DisplayName.Empty();

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Shutdown completed"));

		ShutdownSteamworks();
	}

	void FSteamPlatformCore::Tick()
	{
		if (bIsInitialized)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("SteamworksGamingService: Tick - Running SteamAPI callbacks"));
			SteamAPI_RunCallbacks();
			UE_LOG(LogTemp, VeryVerbose, TEXT("SteamworksGamingService: Tick - Pumping call results"));
			CallResults->Pump();
		}
	}

	void FSteamPlatformCore::InitializeSteamworks()
	{
		SteamErrMsg ErrMsg;
		ESteamAPIInitResult Result = SteamAPI_InitEx(&ErrMsg);
		if (Result != k_ESteamAPIInitResult_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Steam init failed (%d): %hs"), (int32)Result, ErrMsg);
			return;
		}

		if (!SteamUserStats() || !SteamUser() || !SteamUtils() || !SteamFriends() || !SteamRemoteStorage() || !SteamMatchmaking() || !SteamApps())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to get Steam interfaces"));
			SteamAPI_Shutdown();
			return;
		}

		if (SteamUser()->BLoggedOn())
		{
			bIsLoggedIn = true;
			CSteamID SteamID = SteamUser()->GetSteamID();
			UserId = FString::Printf(TEXT("%llu"), SteamID.ConvertToUint64());

			const char* PersonaName = SteamFriends()->GetPersonaName();
			DisplayName = PersonaName ? UTF8_TO_TCHAR(PersonaName) : TEXT("Steam User");

			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: User logged in: %s (ID: %s)"), *DisplayName, *UserId);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: User not logged in to Steam"));
		}

		bIsInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steamworks initialized successfully"));
	}

	void FSteamPlatformCore::ShutdownSteamworks()
	{
		if (bIsInitialized)
		{
			FScopeLock Lock(&CallbackCriticalSection);
			SteamAPI_Shutdown();
			bIsInitialized = false;
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Steamworks shutdown"));
		}
	}

	bool FSteamPlatformCore::IsSteamRunning() const
	{
		return SteamAPI_IsSteamRunning();
	}

	bool FSteamPlatformCore::IsSteamOverlayEnabled() const
	{
		return SteamUtils() ? SteamUtils()->IsOverlayEnabled() : false;
	}
}

#endif // GS_WITH_STEAM

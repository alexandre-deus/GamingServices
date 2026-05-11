#include "GamingServices.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Services/EOSGamingService.h"
#include "Services/SteamworksGamingService.h"
#include "Services/NullGamingService.h"

#ifdef USE_STEAMWORKS
#include "SocketSubsystemModule.h"
#include "NetDriver/MinderaSocketSubsystem.h"
#endif

IMPLEMENT_MODULE(FGamingServicesModule, GamingServices)

void FGamingServicesModule::StartupModule()
{
	// Skip platform SDKs when the editor is running without -game (PIE / normal editor session).
	// FApp::IsGame() is true for cooked targets AND for the editor binary launched with -game
	// (i.e. Standalone Game from the editor), false for PIE / regular editor.
	const bool bUseRealService = FApp::IsGame();

	if (bUseRealService)
	{
#ifdef USE_EOS
		Service = MakeUnique<FEOSGamingService>();
#elif defined(USE_STEAMWORKS)
		Service = MakeUnique<FSteamworksGamingService>();
#else
		Service = MakeUnique<FNullGamingService>();
#endif
	}
	else
	{
		Service = MakeUnique<FNullGamingService>();
	}

	Service->InitializePlatform();

#ifdef USE_STEAMWORKS
	if (bUseRealService)
	{
		FMinderaSocketSubsystem* SocketSubsystem = FMinderaSocketSubsystem::Create();
		if (SocketSubsystem)
		{
			FString Error;
			if (SocketSubsystem->Init(Error))
			{
				bSocketSubsystemEnabled = true;
				FSocketSubsystemModule& SSModule = FModuleManager::LoadModuleChecked<FSocketSubsystemModule>(TEXT("Sockets"));
				SSModule.RegisterSocketSubsystem(MINDERA_SOCKET_SUBSYSTEM_NAME, SocketSubsystem, false);
				UE_LOG(LogTemp, Log, TEXT("GamingServices: Registered MinderaSteam socket subsystem"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GamingServices: Failed to init MinderaSteam socket subsystem: %s"), *Error);
				FMinderaSocketSubsystem::Destroy();
			}
		}
	}
#endif

	// Tear the platform down on engine pre-exit rather than module shutdown.
	// ShutdownModule runs very late in process teardown; by that point Steam often
	// can't deliver the "left game" notification cleanly, so the friends list keeps
	// showing the user as "In-Game" for a long time. OnEnginePreExit fires while
	// the engine is still alive enough for a clean Steam disconnect.
	PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FGamingServicesModule::TearDownPlatform);
}

void FGamingServicesModule::TearDownPlatform()
{
#ifdef USE_STEAMWORKS
	if (bSocketSubsystemEnabled)
	{
		FModuleManager& ModuleManager = FModuleManager::Get();
		if (ModuleManager.IsModuleLoaded(TEXT("Sockets")))
		{
			FSocketSubsystemModule& SSModule = FModuleManager::GetModuleChecked<FSocketSubsystemModule>(TEXT("Sockets"));
			SSModule.UnregisterSocketSubsystem(MINDERA_SOCKET_SUBSYSTEM_NAME);
		}
		FMinderaSocketSubsystem::Destroy();
		UE_LOG(LogTemp, Log, TEXT("GamingServices: Unregistered MinderaSteam socket subsystem"));
		bSocketSubsystemEnabled = false;
	}
#endif

	if (Service)
	{
		Service->DestroyPlatform();
		Service.Reset();
	}
}

void FGamingServicesModule::ShutdownModule()
{
	if (PreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);
		PreExitHandle.Reset();
	}

	// Safety net: if OnEnginePreExit never fired (e.g. abnormal teardown path),
	// still tear the platform down here.
	TearDownPlatform();
}

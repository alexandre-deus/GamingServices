#include "GamingServices.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Native/GamingServiceFactory.h"
#include "Native/Null/NullGamingService.h"

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

	// This module owns the single live platform backend — the decomposed Native/ service selected at
	// build time (Steam/EOS/Null). UGamingPlatformSubsystem consumes and ticks this same instance, so
	// exactly ONE backend ever inits the platform SDK. Outside a real game session (PIE / editor) we
	// force the honest null backend regardless of the configured SDK.
	Service = bUseRealService
		? GamingServices::CreateGamingService()
		: MakeUnique<GamingServices::FNullGamingService>();

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

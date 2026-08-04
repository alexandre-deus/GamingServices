#include "GamingServices.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Native/GamingServiceFactory.h"
#include "Native/Null/NullGamingService.h"

#include "SocketSubsystemModule.h"
#include "NetDriver/MinderaSocketSubsystem.h"

IMPLEMENT_MODULE(FGamingServicesModule, GamingServices)

void FGamingServicesModule::StartupModule()
{
	// Skip platform SDKs when the editor is running without -game (PIE / normal editor session).
	// FApp::IsGame() is true for cooked targets AND for the editor binary launched with -game
	// (i.e. Standalone Game from the editor), false for PIE / regular editor.
	const bool bUseRealService = FApp::IsGame();

	// This module owns the live platform service. Which backend (or combination of backends) that is
	// comes from the profile this build was compiled with (GS_PROFILE, see Native/GamingServiceProfile.h).
	// Every vendored SDK is compiled in and none is bound at link time, so the profile picks an
	// arrangement rather than gating what exists. UGamingPlatformSubsystem consumes and ticks this same
	// instance, so exactly ONE service ever inits the platform SDKs. Outside a real game session
	// (PIE / editor) we force the honest null backend regardless of the profile.
	FGamingServiceConnectParams ConnectParams;
	if (bUseRealService)
	{
		const FGamingServicesRuntimeConfig Config = FGamingServicesRuntimeConfig::Active();
		UE_LOG(LogTemp, Log, TEXT("GamingServices: %s"), *Config.ToString());
		ConnectParams = Config.ConnectParams;
		Service = GamingServices::CreateGamingService(Config);
	}
	else
	{
		Service = MakeUnique<GamingServices::FNullGamingService>();
	}

	Service->InitializePlatform(ConnectParams);

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
				UE_LOG(LogTemp, Log, TEXT("GamingServices: Registered Mindera P2P socket subsystem"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GamingServices: Failed to init Mindera P2P socket subsystem: %s"), *Error);
				FMinderaSocketSubsystem::Destroy();
			}
		}
	}

	// Tear the platform down on engine pre-exit rather than module shutdown.
	// ShutdownModule runs very late in process teardown; by that point Steam often
	// can't deliver the "left game" notification cleanly, so the friends list keeps
	// showing the user as "In-Game" for a long time. OnEnginePreExit fires while
	// the engine is still alive enough for a clean Steam disconnect.
	PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FGamingServicesModule::TearDownPlatform);
}

void FGamingServicesModule::TearDownPlatform()
{
	// Only the platform SDK is torn down on pre-exit (for a clean Steam disconnect). The socket
	// subsystem is deliberately left registered: net-driver UObjects are destroyed later by GC and
	// still call GetSocketSubsystem() during their teardown — unregistering here would null that out
	// and crash. It is unregistered in ShutdownModule instead, after GC has run.
	if (Service)
	{
		Service->DestroyPlatform();
		Service.Reset();
	}
}

void FGamingServicesModule::TearDownSocketSubsystem()
{
	if (bSocketSubsystemEnabled)
	{
		FModuleManager& ModuleManager = FModuleManager::Get();
		if (ModuleManager.IsModuleLoaded(TEXT("Sockets")))
		{
			FSocketSubsystemModule& SSModule = FModuleManager::GetModuleChecked<FSocketSubsystemModule>(TEXT("Sockets"));
			SSModule.UnregisterSocketSubsystem(MINDERA_SOCKET_SUBSYSTEM_NAME);
		}
		FMinderaSocketSubsystem::Destroy();
		UE_LOG(LogTemp, Log, TEXT("GamingServices: Unregistered Mindera P2P socket subsystem"));
		bSocketSubsystemEnabled = false;
	}
}

void FGamingServicesModule::ShutdownModule()
{
	if (PreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);
		PreExitHandle.Reset();
	}

	// Safety net: if OnEnginePreExit never fired (e.g. abnormal teardown path), still tear the platform
	// down here. Then unregister the socket subsystem — by now the engine has GC'd its net drivers, so
	// nothing will call GetSocketSubsystem() after this.
	TearDownPlatform();
	TearDownSocketSubsystem();
}

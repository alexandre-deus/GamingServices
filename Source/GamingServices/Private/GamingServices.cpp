#include "GamingServices.h"
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
	const bool bIsStandalone = !GIsEditor;

#ifdef USE_STEAMWORKS
	if (bIsStandalone)
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

	if (bIsStandalone)
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
}

void FGamingServicesModule::ShutdownModule()
{
	if (Service)
	{
		Service->DestroyPlatform();
		Service.Reset();
	}

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
}

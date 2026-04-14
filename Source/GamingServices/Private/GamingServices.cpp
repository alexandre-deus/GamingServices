#include "GamingServices.h"
#include "SocketSubsystemModule.h"

#ifdef USE_STEAMWORKS
#include "NetDriver/MinderaSocketSubsystem.h"
#endif

IMPLEMENT_MODULE(FGamingServicesModule, GamingServices)

void FGamingServicesModule::StartupModule()
{
#ifdef USE_STEAMWORKS
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
#endif
}

void FGamingServicesModule::ShutdownModule()
{
#ifdef USE_STEAMWORKS
	if (bSocketSubsystemEnabled)
	{
		// Match official pattern: check if Sockets module is still loaded before unregistering
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

#include "Blueprint/GamingPlatformSubsystem.h"

#include "GamingServices.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "Native/IGamingService.h"

void UGamingPlatformSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Borrow the single live backend the module already built and initialized — never build our own,
	// so the platform SDK is initialized exactly once.
	FGamingServicesModule& Module = FModuleManager::GetModuleChecked<FGamingServicesModule>(TEXT("GamingServices"));
	Service = &Module.GetService();
	check(Service);
}

void UGamingPlatformSubsystem::Deinitialize()
{
	// The module owns the service lifetime (torn down on engine pre-exit); just drop our borrowed ref.
	Service = nullptr;

	Super::Deinitialize();
}

void UGamingPlatformSubsystem::Tick(float DeltaTime)
{
	if (Service)
	{
		Service->Tick();
	}
}

TStatId UGamingPlatformSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGamingPlatformSubsystem, STATGROUP_Tickables);
}

IGamingService* UGamingPlatformSubsystem::GetServiceFromContext(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UGamingPlatformSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UGamingPlatformSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetService() : nullptr;
}

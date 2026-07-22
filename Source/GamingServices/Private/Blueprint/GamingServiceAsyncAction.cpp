#include "Blueprint/GamingServiceAsyncAction.h"

#include "Blueprint/GamingPlatformSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

IGamingService* UGamingServiceAsyncAction::ResolveService() const
{
	return UGamingPlatformSubsystem::GetServiceFromContext(WorldContext.Get());
}

void UGamingServiceAsyncAction::KeepAlive()
{
	if (!GEngine)
	{
		return;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			RegisterWithGameInstance(GameInstance);
		}
	}
}

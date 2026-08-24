#include "Blueprint/Libraries/AchievementsLibrary.h"

#include "Blueprint/GamingPlatformSubsystem.h"
#include "Native/IGamingService.h"
#include "Native/Interfaces/IAchievementProgressService.h"
#include "Native/Interfaces/IAchievementsService.h"

namespace
{
	IAchievementProgressService* ResolveProgressService(const UObject* WorldContextObject)
	{
		IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
		return Service ? Service->GetAchievementProgress() : nullptr;
	}

	IAchievementsService* ResolveAchievementsService(const UObject* WorldContextObject)
	{
		IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
		return Service ? Service->GetAchievements() : nullptr;
	}
}

UAsyncAction_UnlockAchievement* UAsyncAction_UnlockAchievement::UnlockAchievement(
	UObject* WorldContextObject, const FString& AchievementId)
{
	UAsyncAction_UnlockAchievement* Action = NewObject<UAsyncAction_UnlockAchievement>();
	Action->WorldContext = WorldContextObject;
	Action->AchievementId = AchievementId;
	return Action;
}

void UAsyncAction_UnlockAchievement::Activate()
{
	IGamingService* Service = ResolveService();
	if (!Service)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	// Prefer the registered-catalogue layer so the id the Blueprint passed is mapped onto whichever
	// platform is live. With nothing registered it resolves ids unchanged, so this is the same call.
	if (IAchievementProgressService* Progress = Service->GetAchievementProgress())
	{
		KeepAlive();
		Progress->UnlockAchievement(AchievementId, [this](const FGamingServiceResult& Result)
		{
			Completed.Broadcast(Result);
			SetReadyToDestroy();
		});
		return;
	}

	IAchievementsService* Achievements = Service->GetAchievements();
	if (!Achievements)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Achievements->UnlockAchievement(AchievementId, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_QueryAchievements* UAsyncAction_QueryAchievements::QueryAchievements(UObject* WorldContextObject)
{
	UAsyncAction_QueryAchievements* Action = NewObject<UAsyncAction_QueryAchievements>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_QueryAchievements::Activate()
{
	IGamingService* Service = ResolveService();
	IAchievementsService* Achievements = Service ? Service->GetAchievements() : nullptr;
	if (!Achievements)
	{
		Completed.Broadcast(FAchievementsQueryResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Achievements->QueryAchievements([this](const FAchievementsQueryResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_ReportAchievementProgress* UAsyncAction_ReportAchievementProgress::ReportAchievementProgress(
	UObject* WorldContextObject, const FString& AchievementId, int32 CurrentValue)
{
	UAsyncAction_ReportAchievementProgress* Action = NewObject<UAsyncAction_ReportAchievementProgress>();
	Action->WorldContext = WorldContextObject;
	Action->AchievementId = AchievementId;
	Action->CurrentValue = CurrentValue;
	return Action;
}

void UAsyncAction_ReportAchievementProgress::Activate()
{
	IGamingService* Service = ResolveService();
	IAchievementProgressService* Progress = Service ? Service->GetAchievementProgress() : nullptr;
	if (!Progress)
	{
		Completed.Broadcast(FAchievementProgressResult(false, AchievementId));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Progress->ReportProgress(AchievementId, CurrentValue, [this](const FAchievementProgressResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_AddAchievementProgress* UAsyncAction_AddAchievementProgress::AddAchievementProgress(
	UObject* WorldContextObject, const FString& AchievementId, int32 Delta)
{
	UAsyncAction_AddAchievementProgress* Action = NewObject<UAsyncAction_AddAchievementProgress>();
	Action->WorldContext = WorldContextObject;
	Action->AchievementId = AchievementId;
	Action->Delta = Delta;
	return Action;
}

void UAsyncAction_AddAchievementProgress::Activate()
{
	IGamingService* Service = ResolveService();
	IAchievementProgressService* Progress = Service ? Service->GetAchievementProgress() : nullptr;
	if (!Progress)
	{
		Completed.Broadcast(FAchievementProgressResult(false, AchievementId));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Progress->AddProgress(AchievementId, Delta, [this](const FAchievementProgressResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_ValidateAchievements* UAsyncAction_ValidateAchievements::ValidateAchievements(
	UObject* WorldContextObject)
{
	UAsyncAction_ValidateAchievements* Action = NewObject<UAsyncAction_ValidateAchievements>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_ValidateAchievements::Activate()
{
	IGamingService* Service = ResolveService();
	IAchievementProgressService* Progress = Service ? Service->GetAchievementProgress() : nullptr;
	if (!Progress)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Progress->ValidateAgainstPlatform([this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_ListenForAchievementUnlocks* UAsyncAction_ListenForAchievementUnlocks::ListenForAchievementUnlocks(
	UObject* WorldContextObject)
{
	UAsyncAction_ListenForAchievementUnlocks* Action = NewObject<UAsyncAction_ListenForAchievementUnlocks>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_ListenForAchievementUnlocks::Activate()
{
	IGamingService* Service = ResolveService();
	IAchievementsService* Achievements = Service ? Service->GetAchievements() : nullptr;
	if (!Achievements)
	{
		SetReadyToDestroy();
		return;
	}

	KeepAlive();

	// The native sink is a single TFunction, so installing one would otherwise displace whatever was
	// listening before. Chain instead: call the previous function, then broadcast. A weak pointer keeps
	// a listener that has since been destroyed from being called while leaving the chain intact for
	// everyone else.
	const TWeakObjectPtr<UAsyncAction_ListenForAchievementUnlocks> WeakThis(this);
	const TWeakObjectPtr<UObject> WeakContext = WorldContext;
	TFunction<void(const FGameAchievement&)> Previous = MoveTemp(Achievements->OnAchievementUnlocked);

	Achievements->OnAchievementUnlocked = [WeakThis, WeakContext, Previous](const FGameAchievement& Achievement)
	{
		if (Previous)
		{
			Previous(Achievement);
		}

		UAsyncAction_ListenForAchievementUnlocks* Action = WeakThis.Get();
		if (!Action)
		{
			return;
		}

		// KeepAlive roots this action until it says otherwise, so it cannot be collected on its own.
		// Release it once the object that asked to listen is gone — a menu widget torn down, a level
		// unloaded — or every activation would leave another rooted action behind for the rest of the
		// session. The link stays in the chain and forwards, but does nothing else.
		if (!WeakContext.IsValid())
		{
			Action->SetReadyToDestroy();
			return;
		}

		Action->Unlocked.Broadcast(Achievement);
	};
}

void UAchievementsLibrary::RegisterAchievements(const UObject* WorldContextObject,
                                                const TArray<FAchievementDefinition>& Definitions)
{
	if (IAchievementProgressService* Progress = ResolveProgressService(WorldContextObject))
	{
		Progress->RegisterAchievements(Definitions);
		return;
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("UAchievementsLibrary: RegisterAchievements had no achievement-progress capability to register "
		       "with — the active backend has no achievements (the null backend, or not signed in to one)"));
}

TArray<FAchievementDefinition> UAchievementsLibrary::GetRegisteredAchievements(const UObject* WorldContextObject)
{
	if (const IAchievementProgressService* Progress = ResolveProgressService(WorldContextObject))
	{
		return Progress->GetRegisteredAchievements();
	}
	return TArray<FAchievementDefinition>();
}

FString UAchievementsLibrary::ResolvePlatformAchievementId(const UObject* WorldContextObject,
                                                           const FString& AchievementId)
{
	if (const IAchievementProgressService* Progress = ResolveProgressService(WorldContextObject))
	{
		return Progress->ResolvePlatformId(AchievementId);
	}
	return AchievementId;
}

bool UAchievementsLibrary::AreAchievementsReady(const UObject* WorldContextObject)
{
	const IAchievementsService* Achievements = ResolveAchievementsService(WorldContextObject);
	return Achievements && Achievements->IsReady();
}

TArray<FGameAchievement> UAchievementsLibrary::GetCachedAchievements(const UObject* WorldContextObject)
{
	if (const IAchievementsService* Achievements = ResolveAchievementsService(WorldContextObject))
	{
		return Achievements->GetCachedAchievements();
	}
	return TArray<FGameAchievement>();
}

bool UAchievementsLibrary::GetCachedAchievement(const UObject* WorldContextObject, const FString& AchievementId,
                                                FGameAchievement& OutAchievement)
{
	OutAchievement = FGameAchievement();

	// Through the progress layer where there is one, so the id is mapped the same way it is for writes.
	if (const IAchievementProgressService* Progress = ResolveProgressService(WorldContextObject))
	{
		return Progress->GetAchievement(AchievementId, OutAchievement);
	}

	const IAchievementsService* Achievements = ResolveAchievementsService(WorldContextObject);
	return Achievements && Achievements->FindCachedAchievement(AchievementId, OutAchievement);
}

int32 UAchievementsLibrary::GetAchievementCount(const TArray<FGameAchievement>& Achievements)
{
	return Achievements.Num();
}

FGameAchievement UAchievementsLibrary::GetAchievementAt(const TArray<FGameAchievement>& Achievements, int32 Index)
{
	return Achievements.IsValidIndex(Index) ? Achievements[Index] : FGameAchievement();
}

float UAchievementsLibrary::GetAchievementProgressFraction(const FGameAchievement& Achievement)
{
	return static_cast<float>(Achievement.Progress);
}

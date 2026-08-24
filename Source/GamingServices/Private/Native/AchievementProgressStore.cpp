#include "Native/AchievementProgressStore.h"

#include "Native/Interfaces/IAchievementsService.h"
#include "Native/Interfaces/IStatsService.h"

void FAchievementProgressStore::RegisterAchievements(const TArray<FAchievementDefinition>& Definitions)
{
	for (const FAchievementDefinition& Definition : Definitions)
	{
		if (Definition.Id.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("FAchievementProgressStore: ignoring a definition with no Id"));
			continue;
		}

		if (const int32* ExistingIndex = IndexById.Find(Definition.Id))
		{
			Registered[*ExistingIndex] = Definition;
		}
		else
		{
			IndexById.Add(Definition.Id, Registered.Add(Definition));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FAchievementProgressStore: %d achievement(s) registered (%d total)"),
	       Definitions.Num(), Registered.Num());
}

bool FAchievementProgressStore::FindDefinition(const FString& Id, FAchievementDefinition& OutDefinition) const
{
	if (const int32* Index = IndexById.Find(Id))
	{
		OutDefinition = Registered[*Index];
		return true;
	}
	return false;
}

bool FAchievementProgressStore::IsAvailableHere(const FAchievementDefinition& Definition) const
{
	// A row that names no platform id at all is using one id everywhere, so it exists everywhere.
	// Once a row names one, the empty ones read as "not on that platform" rather than "same as Id" —
	// otherwise a platform-exclusive achievement would resolve to an id its store has never heard of
	// and fail on every call.
	if (Definition.SteamApiName.IsEmpty() && Definition.EOSAchievementId.IsEmpty())
	{
		return true;
	}

	switch (Backend)
	{
	case EGamingBackend::Steamworks:
		return !Definition.SteamApiName.IsEmpty();
	case EGamingBackend::EpicOnlineServices:
		return !Definition.EOSAchievementId.IsEmpty();
	default:
		return true;
	}
}

FString FAchievementProgressStore::ResolvePlatformId(const FString& Id) const
{
	FAchievementDefinition Definition;
	if (!FindDefinition(Id, Definition))
	{
		// Nothing registered under this id: hand it to the platform as-is. This is what lets a game
		// adopt the catalogue for some achievements and keep passing raw platform ids for the rest.
		return Id;
	}

	if (!IsAvailableHere(Definition))
	{
		return FString();
	}

	switch (Backend)
	{
	case EGamingBackend::Steamworks:
		return Definition.SteamApiName.IsEmpty() ? Definition.Id : Definition.SteamApiName;
	case EGamingBackend::EpicOnlineServices:
		return Definition.EOSAchievementId.IsEmpty() ? Definition.Id : Definition.EOSAchievementId;
	default:
		return Definition.Id;
	}
}

void FAchievementProgressStore::UnlockAchievement(const FString& Id,
                                                  TFunction<void(const FGamingServiceResult&)> Callback)
{
	const FString PlatformId = ResolvePlatformId(Id);
	if (PlatformId.IsEmpty())
	{
		// Registered, but not an achievement this store has. Succeeding is the honest answer: nothing
		// failed and there is nothing the caller could do about it, and reporting failure would push
		// per-platform branching back into gameplay code, which is what this layer exists to avoid.
		UE_LOG(LogTemp, Verbose,
		       TEXT("FAchievementProgressStore: '%s' does not exist on %s; unlock skipped"),
		       *Id, GamingServices::LexToString(Backend));
		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
		return;
	}

	Achievements.UnlockAchievement(PlatformId, MoveTemp(Callback));
}

bool FAchievementProgressStore::GetAchievement(const FString& Id, FGameAchievement& OutAchievement) const
{
	const FString PlatformId = ResolvePlatformId(Id);
	return !PlatformId.IsEmpty() && Achievements.FindCachedAchievement(PlatformId, OutAchievement);
}

void FAchievementProgressStore::Refresh(TFunction<void(const FAchievementsQueryResult&)> Callback)
{
	Achievements.QueryAchievements(MoveTemp(Callback));
}

bool FAchievementProgressStore::ShouldIndicateProgress(int32 PreviousValue, int32 NewValue, int32 TargetValue) const
{
	if (TargetValue <= 0)
	{
		return false;
	}

	const int32 Step = FMath::Max(1, FMath::RoundToInt(TargetValue * ProgressIndicationStep));
	return (NewValue / Step) > (PreviousValue / Step);
}

void FAchievementProgressStore::CompleteProgress(const FAchievementDefinition& Definition, int32 NewValue,
                                                 int32 PreviousValue,
                                                 TFunction<void(const FAchievementProgressResult&)> Callback)
{
	const FString PlatformId = ResolvePlatformId(Definition.Id);
	const bool bReachedTarget = Definition.TargetValue > 0 && NewValue >= Definition.TargetValue;

	if (!bReachedTarget)
	{
		if (ShouldIndicateProgress(PreviousValue, NewValue, Definition.TargetValue))
		{
			Achievements.IndicateProgress(PlatformId, NewValue, Definition.TargetValue, {});
		}
		if (Callback)
		{
			Callback(FAchievementProgressResult(true, Definition.Id, NewValue, Definition.TargetValue));
		}
		return;
	}

	// Already unlocked (an earlier run, another device, or the platform's own threshold got there
	// first). Re-unlocking would succeed harmlessly on both backends, but reporting bUnlockedNow
	// again would make a caller fire its "achievement earned" moment a second time.
	FGameAchievement Cached;
	if (Achievements.FindCachedAchievement(PlatformId, Cached) && Cached.bIsUnlocked)
	{
		if (Callback)
		{
			Callback(FAchievementProgressResult(true, Definition.Id, NewValue, Definition.TargetValue));
		}
		return;
	}

	const FString Id = Definition.Id;
	const int32 TargetValue = Definition.TargetValue;
	Achievements.UnlockAchievement(PlatformId,
	                               [Id, NewValue, TargetValue, Callback](const FGamingServiceResult& Result)
	                               {
		                               if (Callback)
		                               {
			                               Callback(FAchievementProgressResult(Result.bSuccess, Id, NewValue,
			                                                                  TargetValue, Result.bSuccess));
		                               }
	                               });
}

void FAchievementProgressStore::ReportProgress(const FString& Id, int32 CurrentValue,
                                               TFunction<void(const FAchievementProgressResult&)> Callback)
{
	FAchievementDefinition Definition;
	if (!FindDefinition(Id, Definition))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FAchievementProgressStore: ReportProgress for unregistered achievement '%s'; register it "
			       "(or call UnlockAchievement directly) before reporting progress"),
		       *Id);
		if (Callback)
		{
			Callback(FAchievementProgressResult(false, Id));
		}
		return;
	}

	if (!IsAvailableHere(Definition))
	{
		// Not an achievement this store has. Skip the stat too: it exists to drive this achievement, and
		// writing it on a platform where nothing reads it would only risk a failure the caller cannot
		// act on. Reported as a success carrying the value the caller asked for.
		UE_LOG(LogTemp, Verbose,
		       TEXT("FAchievementProgressStore: '%s' does not exist on %s; progress skipped"),
		       *Id, GamingServices::LexToString(Backend));
		if (Callback)
		{
			Callback(FAchievementProgressResult(true, Id, CurrentValue, Definition.TargetValue));
		}
		return;
	}

	// No stat behind this achievement, so there is nothing to count — the caller's own number is the
	// only measure of progress there is. Honour it as an unlock once it reaches the target.
	if (!Definition.IsProgressive())
	{
		if (Definition.TargetValue <= 0)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("FAchievementProgressStore: '%s' is registered without a stat or a target, so it has no "
				       "progress to report; unlock it directly instead"),
			       *Id);
			if (Callback)
			{
				Callback(FAchievementProgressResult(false, Id));
			}
			return;
		}

		CompleteProgress(Definition, CurrentValue, CurrentValue, MoveTemp(Callback));
		return;
	}

	if (!Stats)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FAchievementProgressStore: '%s' counts stat '%s' but this backend has no stats capability"),
		       *Id, *Definition.StatName);
		if (Callback)
		{
			Callback(FAchievementProgressResult(false, Id));
		}
		return;
	}

	Stats->QueryStat(Definition.StatName,
	                 [this, Definition, CurrentValue, Callback](const FStatQueryResult& Query)
	                 {
		                 // A stat that has never been written reads back as absent on some backends;
		                 // treating that as zero is right, and a genuinely broken stat surfaces as an
		                 // ingest failure below rather than being guessed at here.
		                 const int32 StoredValue = Query.bSuccess ? Query.Value : 0;
		                 const int32 Baseline = GetBaseline(Definition.StatName, StoredValue);

		                 // Reporting a total the player has already reached — or one an ingest still in
		                 // flight will reach — is the normal case for a caller reporting its running
		                 // total on every event. Nothing to ingest, but still check the threshold so an
		                 // unlock that failed earlier heals itself.
		                 if (CurrentValue <= Baseline)
		                 {
			                 CompleteProgress(Definition, Baseline, Baseline, Callback);
			                 return;
		                 }

		                 IngestAndComplete(Definition, Baseline, CurrentValue, Callback);
	                 });
}

int32 FAchievementProgressStore::GetBaseline(const FString& StatName, int32 StoredValue) const
{
	const FClaimedStat* Claimed = ClaimedStats.Find(StatName);

	// The stored value wins when the platform has already caught up past the claim, which is the normal
	// state once the ingests land.
	return Claimed ? FMath::Max(StoredValue, Claimed->Total) : StoredValue;
}

void FAchievementProgressStore::IngestAndComplete(const FAchievementDefinition& Definition, int32 Baseline,
                                                  int32 NewTotal,
                                                  TFunction<void(const FAchievementProgressResult&)> Callback)
{
	const int32 Delta = NewTotal - Baseline;

	// Claim BEFORE issuing the ingest: a report that starts while this one is still in flight has to
	// measure itself against the total this one is on its way to reaching, not the stale stored value.
	// Scoped, because IngestStat completes synchronously on some backends and its callback erases this
	// very entry — a reference held across the call would dangle.
	{
		FClaimedStat& Claimed = ClaimedStats.FindOrAdd(Definition.StatName);
		Claimed.Total = FMath::Max(Claimed.Total, NewTotal);
		++Claimed.InFlight;
	}

	const FString StatName = Definition.StatName;
	Stats->IngestStat(StatName, Delta,
	                  [this, Definition, StatName, Baseline, NewTotal, Delta, Callback](
	                  const FGamingServiceResult& Ingest)
	                  {
		                  // Release the claim once nothing is outstanding for this stat, so a later
		                  // platform-side reset is not shadowed by a stale claim for the whole session.
		                  if (FClaimedStat* Pending = ClaimedStats.Find(StatName))
		                  {
			                  if (--Pending->InFlight <= 0)
			                  {
				                  ClaimedStats.Remove(StatName);
			                  }
		                  }

		                  if (!Ingest.bSuccess)
		                  {
			                  UE_LOG(LogTemp, Warning,
			                         TEXT("FAchievementProgressStore: failed to ingest %d into stat '%s' for '%s'"),
			                         Delta, *StatName, *Definition.Id);
			                  if (Callback)
			                  {
				                  Callback(FAchievementProgressResult(false, Definition.Id, Baseline,
				                                                     Definition.TargetValue));
			                  }
			                  return;
		                  }

		                  CompleteProgress(Definition, NewTotal, Baseline, Callback);
	                  });
}

void FAchievementProgressStore::AddProgress(const FString& Id, int32 Delta,
                                            TFunction<void(const FAchievementProgressResult&)> Callback)
{
	FAchievementDefinition Definition;
	if (!FindDefinition(Id, Definition) || !Definition.IsProgressive())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FAchievementProgressStore: AddProgress needs '%s' registered with a stat and a target"), *Id);
		if (Callback)
		{
			Callback(FAchievementProgressResult(false, Id));
		}
		return;
	}

	if (!IsAvailableHere(Definition))
	{
		UE_LOG(LogTemp, Verbose, TEXT("FAchievementProgressStore: '%s' does not exist on %s; progress skipped"),
		       *Id, GamingServices::LexToString(Backend));
		if (Callback)
		{
			Callback(FAchievementProgressResult(true, Id, 0, Definition.TargetValue));
		}
		return;
	}

	if (!Stats)
	{
		if (Callback)
		{
			Callback(FAchievementProgressResult(false, Id));
		}
		return;
	}

	// Resolve the increment against the stored total and claim the sum here rather than handing the
	// absolute value back to ReportProgress: that would re-read the stat, and two increments issued
	// close together would then both resolve to the same total and one of them would be dropped.
	Stats->QueryStat(Definition.StatName, [this, Definition, Delta, Callback](const FStatQueryResult& Query)
	{
		const int32 StoredValue = Query.bSuccess ? Query.Value : 0;
		const int32 Baseline = GetBaseline(Definition.StatName, StoredValue);

		if (Delta <= 0)
		{
			// Nothing to add, but still worth checking the threshold so a failed unlock heals.
			CompleteProgress(Definition, Baseline, Baseline, Callback);
			return;
		}

		IngestAndComplete(Definition, Baseline, Baseline + Delta, Callback);
	});
}

void FAchievementProgressStore::ValidateAgainstPlatform(TFunction<void(const FGamingServiceResult&)> Callback)
{
	Refresh([this, Callback](const FAchievementsQueryResult& Query)
	{
		if (!Query.bSuccess)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("FAchievementProgressStore: cannot validate the catalogue — the platform query failed"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		int32 Problems = 0;

		int32 Skipped = 0;

		for (const FAchievementDefinition& Definition : Registered)
		{
			const FString PlatformId = ResolvePlatformId(Definition.Id);
			if (PlatformId.IsEmpty())
			{
				// Deliberately not on this platform. Warning about it here would bury the real drift
				// under one line per exclusive achievement every time this runs.
				++Skipped;
				continue;
			}

			const FGameAchievement* Found = Query.Achievements.FindByPredicate(
				[&PlatformId](const FGameAchievement& Achievement) { return Achievement.Id == PlatformId; });

			if (!Found)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("FAchievementProgressStore: '%s' maps to platform id '%s', which %s does not publish "
					       "— it will never unlock"),
				       *Definition.Id, *PlatformId, GamingServices::LexToString(Backend));
				++Problems;
				continue;
			}

			if (Definition.IsProgressive() && Found->TargetValue > 0 && Found->TargetValue != Definition.TargetValue)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("FAchievementProgressStore: '%s' is registered with target %d but %s configures %d"),
				       *Definition.Id, Definition.TargetValue, GamingServices::LexToString(Backend),
				       Found->TargetValue);
				++Problems;
			}

			if (Definition.IsProgressive() && Found->StatProgress.Num() > 0)
			{
				const bool bStatMatches = Found->StatProgress.ContainsByPredicate(
					[&Definition](const FAchievementStatProgress& Stat)
					{
						return Stat.StatName.Equals(Definition.StatName, ESearchCase::IgnoreCase);
					});
				if (!bStatMatches)
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("FAchievementProgressStore: '%s' counts stat '%s', which is not one of the stats "
						       "%s gates it on — progress will be written somewhere that unlocks nothing"),
					       *Definition.Id, *Definition.StatName, GamingServices::LexToString(Backend));
					++Problems;
				}
			}
		}

		// Only meaningful once a catalogue exists; with none registered every platform achievement is
		// legitimately reached by its raw id.
		if (Registered.Num() > 0)
		{
			for (const FGameAchievement& Achievement : Query.Achievements)
			{
				const bool bRegistered = Registered.ContainsByPredicate(
					[this, &Achievement](const FAchievementDefinition& Definition)
					{
						return ResolvePlatformId(Definition.Id) == Achievement.Id;
					});
				if (!bRegistered)
				{
					UE_LOG(LogTemp, Log,
					       TEXT("FAchievementProgressStore: %s publishes '%s', which is not in the registered "
						       "catalogue"),
					       GamingServices::LexToString(Backend), *Achievement.Id);
				}
			}
		}

		UE_LOG(LogTemp, Log,
		       TEXT("FAchievementProgressStore: validated %d registered achievement(s) against %d published by %s "
			       "— %d problem(s), %d not on this platform"),
		       Registered.Num() - Skipped, Query.Achievements.Num(), GamingServices::LexToString(Backend),
		       Problems, Skipped);

		if (Callback)
		{
			Callback(FGamingServiceResult(Problems == 0));
		}
	});
}

#pragma once

#include "CoreMinimal.h"
#include "Native/GamingBackend.h"
#include "Native/Interfaces/IAchievementProgressService.h"

class IAchievementsService;
class IStatsService;

/**
 * Backend-agnostic IAchievementProgressService implemented on top of any IAchievementsService plus
 * that backend's IStatsService.
 *
 * This is the achievements counterpart of FRemoteSettingsStore: one shared implementation that every
 * real backend reuses instead of re-deriving progression rules, so achievement-progress support
 * tracks achievement support and behaves identically on Steam and EOS.
 *
 * What it owns:
 *
 *   - The registered catalogue (FAchievementDefinition rows), and the mapping from the game's own id
 *     onto the live backend's platform id. With nothing registered every id passes through unchanged.
 *
 *   - The progression rule. Both platforms count progress in stats, so ReportProgress ingests into
 *     the registered stat, drives the platform's own progress toast at milestones, and unlocks at the
 *     threshold. The unlock is always issued explicitly, even on EOS, whose server would eventually
 *     unlock from the threshold by itself: an explicit unlock is idempotent there, and issuing it
 *     makes the moment the player earns an achievement identical on both platforms rather than
 *     "immediately here, whenever the server notices there".
 *
 * What it deliberately does NOT own: unlock notifications. Those are a sink on IAchievementsService
 * (OnAchievementUnlocked), which reports platform-side unlocks this store never issued, and a single
 * TFunction sink cannot be shared. Bind it there.
 *
 * Stat semantics this relies on: IStatsService::IngestStat is additive on every backend (Steam reads
 * and re-sets, EOS ingests a delta). Absolute progress is therefore written as a delta against the
 * stored value, which requires an EOS stat configured with SUM aggregation. A MAX/LATEST stat should
 * be driven with the raw IStatsService instead.
 */
class GAMINGSERVICES_API FAchievementProgressStore final : public IAchievementProgressService
{
public:
	/**
	 * Achievements must outlive this store (the owning service guarantees this). Stats may be null on
	 * a backend without a stats capability, which leaves only outright unlocks working.
	 */
	FAchievementProgressStore(IAchievementsService& InAchievements, IStatsService* InStats, EGamingBackend InBackend)
		: Achievements(InAchievements)
		  , Stats(InStats)
		  , Backend(InBackend)
	{
	}

	virtual void RegisterAchievements(const TArray<FAchievementDefinition>& Definitions) override;
	virtual const TArray<FAchievementDefinition>& GetRegisteredAchievements() const override { return Registered; }
	virtual bool FindDefinition(const FString& Id, FAchievementDefinition& OutDefinition) const override;
	virtual FString ResolvePlatformId(const FString& Id) const override;

	virtual void UnlockAchievement(const FString& Id,
	                               TFunction<void(const FGamingServiceResult&)> Callback) override;
	virtual void ReportProgress(const FString& Id, int32 CurrentValue,
	                            TFunction<void(const FAchievementProgressResult&)> Callback) override;
	virtual void AddProgress(const FString& Id, int32 Delta,
	                         TFunction<void(const FAchievementProgressResult&)> Callback) override;

	virtual bool GetAchievement(const FString& Id, FGameAchievement& OutAchievement) const override;
	virtual void Refresh(TFunction<void(const FAchievementsQueryResult&)> Callback) override;
	virtual void ValidateAgainstPlatform(TFunction<void(const FGamingServiceResult&)> Callback) override;

private:
	/**
	 * Fraction of the target that must be gained before the platform's progress toast is shown again.
	 * Steam rate-limits these and silently drops the excess, so indicating on every increment would
	 * cost the player the milestones they should have seen.
	 */
	static constexpr float ProgressIndicationStep = 0.1f;

	/**
	 * A stat total that has been claimed by ingests already issued, whether or not they have landed yet.
	 *
	 * Progress is written as a delta against the stored value, and on a backend where both the read and
	 * the write are network round trips (EOS) two reports issued close together would otherwise both
	 * measure themselves against the same pre-ingest value and each ingest its full delta — reporting
	 * totals 1, 2, 3 in quick succession would add 6. Claiming the new total before issuing the ingest
	 * makes the next report measure against what is already on its way instead.
	 */
	struct FClaimedStat
	{
		int32 Total = 0;

		/** Ingests issued and not yet finished. The claim is dropped when this reaches zero, so a stat
		 *  reset on the platform is not shadowed by a stale claim for the rest of the session. */
		int32 InFlight = 0;
	};

	/**
	 * Whether this achievement exists on the backend that is running. False for a platform-exclusive
	 * achievement seen from the wrong platform — see FAchievementDefinition::SteamApiName for the rule.
	 */
	bool IsAvailableHere(const FAchievementDefinition& Definition) const;

	/** The claimed total for a stat, or StoredValue when nothing is in flight for it. */
	int32 GetBaseline(const FString& StatName, int32 StoredValue) const;

	/** Claims NewTotal, ingests the difference, and finishes the report. Shared by both entry points. */
	void IngestAndComplete(const FAchievementDefinition& Definition, int32 Baseline, int32 NewTotal,
	                       TFunction<void(const FAchievementProgressResult&)> Callback);

	/** Writes the ingested value through and finishes the report (unlock at the threshold, toast, sink). */
	void CompleteProgress(const FAchievementDefinition& Definition, int32 NewValue, int32 PreviousValue,
	                      TFunction<void(const FAchievementProgressResult&)> Callback);

	/** Whether crossing from Previous to New passes the next milestone worth a platform toast. */
	bool ShouldIndicateProgress(int32 PreviousValue, int32 NewValue, int32 TargetValue) const;

	IAchievementsService& Achievements;
	IStatsService* Stats = nullptr;
	EGamingBackend Backend = EGamingBackend::None;

	TArray<FAchievementDefinition> Registered;

	/** Index into Registered by the game's own id, kept in step by RegisterAchievements. */
	TMap<FString, int32> IndexById;

	/** Keyed by stat name. Only holds entries while ingests for that stat are outstanding. */
	TMap<FString, FClaimedStat> ClaimedStats;
};

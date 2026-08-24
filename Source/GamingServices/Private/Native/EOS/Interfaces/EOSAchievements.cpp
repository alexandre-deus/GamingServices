#if defined(GS_WITH_EOS)

#include "Native/EOS/Interfaces/EOSAchievements.h"
#include "EOSCommon.h"
#include "EOSCallbackContext.h"

#include <string>

namespace GamingServices
{
	using FAchievementUnlockCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSAchievements>;
	using FAchievementsQueryCallbackCtx = TEOSCallbackContext<FAchievementsQueryResult, FEOSAchievements>;

	namespace
	{
		// Cast the core's opaque accessor back to its EOS_* type here so the core header stays SDK-free.
		EOS_HAchievements AchievementsHandle(const FEOSPlatformCore& Core)
		{
			return static_cast<EOS_HAchievements>(Core.GetAchievementsHandle());
		}

		FString Utf8OrEmpty(const char* Text)
		{
			return Text ? FString(UTF8_TO_TCHAR(Text)) : FString();
		}

		/**
		 * Folds a player's per-stat standing into the single CurrentValue/TargetValue pair the common
		 * struct reports. Every stat threshold has to be satisfied for the achievement to unlock, so the
		 * one furthest from its threshold is the one that actually gates it — reporting the most complete
		 * stat instead would show a player at "9/10" on an achievement they have barely started.
		 */
		void ApplyStatProgress(const EOS_Achievements_PlayerAchievement& PlayerAchievement, FGameAchievement& Out)
		{
			double GatingFraction = TNumericLimits<double>::Max();

			for (int32 Index = 0; Index < PlayerAchievement.StatInfoCount; ++Index)
			{
				const EOS_Achievements_PlayerStatInfo& StatInfo = PlayerAchievement.StatInfo[Index];

				FAchievementStatProgress Progress;
				Progress.StatName = Utf8OrEmpty(StatInfo.Name);
				Progress.CurrentValue = StatInfo.CurrentValue;
				Progress.ThresholdValue = StatInfo.ThresholdValue;
				Out.StatProgress.Add(Progress);

				if (StatInfo.ThresholdValue <= 0)
				{
					continue;
				}

				const double Fraction = static_cast<double>(StatInfo.CurrentValue) /
					static_cast<double>(StatInfo.ThresholdValue);
				if (Fraction < GatingFraction)
				{
					GatingFraction = Fraction;
					Out.CurrentValue = StatInfo.CurrentValue;
					Out.TargetValue = StatInfo.ThresholdValue;
				}
			}
		}

		/** Map an EOS achievement definition + player-progress record onto the game's achievement struct. */
		void ConvertEOSAchievementToGameAchievement(const EOS_Achievements_DefinitionV2* EOSDefinition,
		                                            const EOS_Achievements_PlayerAchievement* EOSPlayerAchievement,
		                                            FGameAchievement& GameAchievement)
		{
			if (EOSDefinition)
			{
				GameAchievement.Id = Utf8OrEmpty(EOSDefinition->AchievementId);
				GameAchievement.DisplayName = Utf8OrEmpty(EOSDefinition->UnlockedDisplayName);
				GameAchievement.Description = Utf8OrEmpty(EOSDefinition->UnlockedDescription);
				GameAchievement.FlavorText = Utf8OrEmpty(EOSDefinition->FlavorText);
				GameAchievement.bIsHidden = (EOSDefinition->bIsHidden == EOS_TRUE);

				// The definition's thresholds describe the achievement even for a player who has never
				// touched it, so seed the target from them; the player record refines it below.
				for (uint32 Index = 0; Index < EOSDefinition->StatThresholdsCount; ++Index)
				{
					const EOS_Achievements_StatThresholds& Threshold = EOSDefinition->StatThresholds[Index];
					if (Threshold.Threshold > GameAchievement.TargetValue)
					{
						GameAchievement.TargetValue = Threshold.Threshold;
					}
				}
			}

			if (EOSPlayerAchievement)
			{
				// Locked achievements report EOS_ACHIEVEMENTS_ACHIEVEMENT_UNLOCKTIME_UNDEFINED (-1), so a
				// plain "!= 0" check would read locked as unlocked.
				GameAchievement.bIsUnlocked =
					(EOSPlayerAchievement->UnlockTime != EOS_ACHIEVEMENTS_ACHIEVEMENT_UNLOCKTIME_UNDEFINED);

				if (GameAchievement.bIsUnlocked)
				{
					GameAchievement.UnlockTime =
						FDateTime::FromUnixTimestamp(static_cast<int64>(EOSPlayerAchievement->UnlockTime));
				}

				// The player record carries text and icons resolved against this player's current
				// progress, which is what a progress-aware achievement screen wants; fall back to the
				// definition's unlocked strings when the portal has not configured the progress variants.
				const FString PlayerDisplayName = Utf8OrEmpty(EOSPlayerAchievement->DisplayName);
				if (!PlayerDisplayName.IsEmpty())
				{
					GameAchievement.DisplayName = PlayerDisplayName;
				}
				const FString PlayerDescription = Utf8OrEmpty(EOSPlayerAchievement->Description);
				if (!PlayerDescription.IsEmpty())
				{
					GameAchievement.Description = PlayerDescription;
				}
				GameAchievement.IconUrl = Utf8OrEmpty(EOSPlayerAchievement->IconURL);
				if (GameAchievement.FlavorText.IsEmpty())
				{
					GameAchievement.FlavorText = Utf8OrEmpty(EOSPlayerAchievement->FlavorText);
				}

				GameAchievement.StatProgress.Reset();
				GameAchievement.CurrentValue = 0;
				ApplyStatProgress(*EOSPlayerAchievement, GameAchievement);
			}

			if (EOSDefinition && GameAchievement.IconUrl.IsEmpty())
			{
				const char* Icon = GameAchievement.bIsUnlocked
					                   ? EOSDefinition->UnlockedIconURL
					                   : EOSDefinition->LockedIconURL;
				GameAchievement.IconUrl = Utf8OrEmpty(Icon);
			}

			if (GameAchievement.bIsUnlocked)
			{
				GameAchievement.Progress = 1.0;
				if (GameAchievement.TargetValue > 0)
				{
					GameAchievement.CurrentValue = FMath::Max(GameAchievement.CurrentValue,
					                                          GameAchievement.TargetValue);
				}
			}
			else if (GameAchievement.TargetValue > 0)
			{
				GameAchievement.Progress = FMath::Clamp(
					static_cast<double>(GameAchievement.CurrentValue) /
					static_cast<double>(GameAchievement.TargetValue), 0.0, 1.0);
			}
			else if (EOSPlayerAchievement)
			{
				// No stat thresholds to measure against, so fall back to the record's own figure. EOS
				// documents it as a percentage but returns a 0..1 fraction in practice, so accept both
				// rather than reporting 100x progress if that ever changes.
				const double RawProgress = EOSPlayerAchievement->Progress;
				GameAchievement.Progress = FMath::Clamp(RawProgress > 1.0 ? RawProgress / 100.0 : RawProgress,
				                                        0.0, 1.0);
			}
		}
	}

	FEOSAchievements::FEOSAchievements(FEOSPlatformCore& InCore)
		: Core(InCore)
	{
		// The unlocked notification belongs here, but registering it needs the achievements handle and
		// the ProductUserId that only exist between login and shutdown. Bind the core's hooks so the
		// core drives the timing while this class owns the work.
		Core.RegisterAchievementsNotificationsHook = [this]() { RegisterUnlockNotifications(); };
		Core.UnregisterAchievementsNotificationsHook = [this]() { UnregisterUnlockNotifications(); };
	}

	FEOSAchievements::~FEOSAchievements()
	{
		UnregisterUnlockNotifications();
	}

	bool FEOSAchievements::IsReady() const
	{
		return Core.IsInitialized() && Core.IsLoggedIn() && Core.GetAchievementsHandle() != nullptr
			&& Core.GetProductUserId() != nullptr;
	}

	void FEOSAchievements::RegisterUnlockNotifications()
	{
		if (UnlockNotificationId != 0 || !Core.GetAchievementsHandle())
		{
			return;
		}

		EOS_Achievements_AddNotifyAchievementsUnlockedV2Options Options = {};
		Options.ApiVersion = EOS_ACHIEVEMENTS_ADDNOTIFYACHIEVEMENTSUNLOCKEDV2_API_LATEST;

		UnlockNotificationId = static_cast<uint64>(EOS_Achievements_AddNotifyAchievementsUnlockedV2(
			AchievementsHandle(Core),
			&Options,
			this,
			[](const EOS_Achievements_OnAchievementsUnlockedCallbackV2Info* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* Self = static_cast<FEOSAchievements*>(Data->ClientData);
				Self->ReportUnlocked(Utf8OrEmpty(Data->AchievementId), static_cast<int64>(Data->UnlockTime));
			}));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: achievement unlock notifications %s"),
		       UnlockNotificationId != 0 ? TEXT("registered") : TEXT("could not be registered"));
	}

	void FEOSAchievements::UnregisterUnlockNotifications()
	{
		if (UnlockNotificationId == 0)
		{
			return;
		}

		if (Core.GetAchievementsHandle())
		{
			EOS_Achievements_RemoveNotifyAchievementsUnlocked(AchievementsHandle(Core),
			                                                  static_cast<EOS_NotificationId>(UnlockNotificationId));
		}
		UnlockNotificationId = 0;
	}

	void FEOSAchievements::RebuildCache()
	{
		TArray<FGameAchievement> Refreshed;

		for (const void* DefinitionPtr : Core.GetAchievementDefinitionPtrs())
		{
			const auto* Definition = static_cast<const EOS_Achievements_DefinitionV2*>(DefinitionPtr);
			if (!Definition)
			{
				continue;
			}

			// The player query cached this user's progress; copy it per achievement so bIsUnlocked,
			// Progress and the stat breakdown reflect the player rather than the bare definition.
			EOS_Achievements_CopyPlayerAchievementByAchievementIdOptions CopyOptions = {};
			CopyOptions.ApiVersion = EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYACHIEVEMENTID_API_LATEST;
			CopyOptions.TargetUserId = ProductUserId(Core);
			CopyOptions.LocalUserId = ProductUserId(Core);
			CopyOptions.AchievementId = Definition->AchievementId;

			EOS_Achievements_PlayerAchievement* PlayerAchievement = nullptr;
			if (EOS_Achievements_CopyPlayerAchievementByAchievementId(
				AchievementsHandle(Core), &CopyOptions, &PlayerAchievement) != EOS_EResult::EOS_Success)
			{
				PlayerAchievement = nullptr;
			}

			FGameAchievement GameAchievement;
			ConvertEOSAchievementToGameAchievement(Definition, PlayerAchievement, GameAchievement);
			Refreshed.Add(MoveTemp(GameAchievement));

			if (PlayerAchievement)
			{
				EOS_Achievements_PlayerAchievement_Release(PlayerAchievement);
			}
		}

		Cached = MoveTemp(Refreshed);
	}

	void FEOSAchievements::ReportUnlocked(const FString& AchievementId, int64 UnlockTimeUnix)
	{
		FGameAchievement* Achievement = Cached.FindByPredicate(
			[&AchievementId](const FGameAchievement& Candidate) { return Candidate.Id == AchievementId; });

		if (!Achievement)
		{
			// Unlocked before anything was queried (or for an achievement added since). Rebuild from the
			// definitions we do have rather than reporting an empty achievement to the game.
			RebuildCache();
			Achievement = Cached.FindByPredicate(
				[&AchievementId](const FGameAchievement& Candidate) { return Candidate.Id == AchievementId; });
		}

		FGameAchievement Unlocked;
		if (Achievement)
		{
			Achievement->bIsUnlocked = true;
			Achievement->Progress = 1.0;
			Achievement->UnlockTime = FDateTime::FromUnixTimestamp(UnlockTimeUnix);
			if (Achievement->TargetValue > 0)
			{
				Achievement->CurrentValue = FMath::Max(Achievement->CurrentValue, Achievement->TargetValue);
			}
			Unlocked = *Achievement;
		}
		else
		{
			Unlocked.Id = AchievementId;
			Unlocked.bIsUnlocked = true;
			Unlocked.Progress = 1.0;
			Unlocked.UnlockTime = FDateTime::FromUnixTimestamp(UnlockTimeUnix);
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: achievement unlocked: %s"), *AchievementId);

		if (OnAchievementUnlocked)
		{
			OnAchievementUnlocked(Unlocked);
		}
		if (OnAchievementsChanged)
		{
			OnAchievementsChanged();
		}
	}

	void FEOSAchievements::UnlockAchievement(const FString& AchievementId,
	                                         TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (!ensureMsgf(IsReady(), TEXT("EOSGamingService: UnlockAchievement called when the service is not ready")))
		{
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Unlocking achievement: %s"), *AchievementId);

		EOS_Achievements_UnlockAchievementsOptions UnlockOptions = {};
		UnlockOptions.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST;
		UnlockOptions.UserId = ProductUserId(Core);

		// Must outlive the EOS_Achievements_UnlockAchievements call below; assigning TCHAR_TO_UTF8()
		// directly leaves a dangling pointer.
		const std::string AchievementIdUtf8 = TCHAR_TO_UTF8(*AchievementId);
		const char* AchievementIds[] = {AchievementIdUtf8.c_str()};
		UnlockOptions.AchievementIds = AchievementIds;
		UnlockOptions.AchievementsCount = 1;

		auto* Ctx = FAchievementUnlockCallbackCtx::Create(this, MoveTemp(Callback));
		EOS_Achievements_UnlockAchievements(
			AchievementsHandle(Core),
			&UnlockOptions,
			Ctx,
			[](const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAchievementUnlockCallbackCtx*>(Data->ClientData);
				FGamingServiceResult Result((Data->ResultCode == EOS_EResult::EOS_Success));
				if (Result.bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Achievement unlocked successfully"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to unlock achievement: %d"),
					       (int32)Data->ResultCode);
				}
				FAchievementUnlockCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void FEOSAchievements::QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
	{
		if (!ensureMsgf(IsReady(), TEXT("EOSGamingService: QueryAchievements called when the service is not ready")))
		{
			if (Callback)
			{
				Callback(FAchievementsQueryResult(false));
			}
			return;
		}

		auto* Ctx = FAchievementsQueryCallbackCtx::Create(this, MoveTemp(Callback));

		EOS_Achievements_QueryPlayerAchievementsOptions PlayerOpts = {};
		PlayerOpts.ApiVersion = EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST;
		PlayerOpts.LocalUserId = ProductUserId(Core);
		PlayerOpts.TargetUserId = ProductUserId(Core);

		EOS_Achievements_QueryPlayerAchievements(
			AchievementsHandle(Core),
			&PlayerOpts,
			Ctx,
			[](const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FAchievementsQueryCallbackCtx*>(Data->ClientData);
				FEOSAchievements* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);

				FAchievementsQueryResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!Result.bSuccess)
				{
					FAchievementsQueryCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				Self->RebuildCache();
				Result.Achievements = Self->Cached;

				if (Self->OnAchievementsChanged)
				{
					Self->OnAchievementsChanged();
				}

				FAchievementsQueryCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}
}

#endif // GS_WITH_EOS

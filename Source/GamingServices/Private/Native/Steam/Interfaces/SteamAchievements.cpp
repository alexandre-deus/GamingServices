#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamAchievements.h"
#include "Native/Steam/SteamPlatformCore.h"
#include "SteamCallResultManager.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	namespace
	{
		/** Steam reports "hidden" as a display attribute whose value is the string "0" or "1". */
		bool ReadHiddenAttribute(ISteamUserStats& UserStats, const char* AchievementId)
		{
			const char* Hidden = UserStats.GetAchievementDisplayAttribute(AchievementId, "hidden");
			return Hidden != nullptr && FCStringAnsi::Atoi(Hidden) != 0;
		}
	}

	struct FSteamAchievements::FImpl
	{
		FSteamAchievements& Owner;

		TArray<FGameAchievement> Cached;

		/**
		 * Whether Steam has delivered the global unlock percentages. Until it has,
		 * GetAchievementAchievedPercent returns false for every achievement and the field stays -1 —
		 * which is exactly why GlobalUnlockPercent must not be confused with the player's own
		 * progress: reading it without this request yields a hardcoded zero.
		 */
		bool bGlobalPercentagesReady = false;
		bool bGlobalPercentagesRequested = false;

		CCallback<FImpl, UserAchievementStored_t> m_CallbackAchievementStored;
		CCallback<FImpl, UserStatsReceived_t> m_CallbackUserStatsReceived;

		explicit FImpl(FSteamAchievements& InOwner)
			: Owner(InOwner)
			  , m_CallbackAchievementStored(this, &FImpl::OnAchievementStored)
			  , m_CallbackUserStatsReceived(this, &FImpl::OnUserStatsReceived)
		{
		}

		FGameAchievement* FindCached(const FString& AchievementId)
		{
			return Cached.FindByPredicate(
				[&AchievementId](const FGameAchievement& Achievement) { return Achievement.Id == AchievementId; });
		}

		/** Reads everything Steam knows about one achievement into the common struct. */
		void ReadAchievement(ISteamUserStats& UserStats, const char* AchievementId, FGameAchievement& Out) const
		{
			Out.Id = UTF8_TO_TCHAR(AchievementId);

			const char* DisplayName = UserStats.GetAchievementDisplayAttribute(AchievementId, "name");
			Out.DisplayName = DisplayName ? UTF8_TO_TCHAR(DisplayName) : Out.Id;

			const char* Description = UserStats.GetAchievementDisplayAttribute(AchievementId, "desc");
			Out.Description = Description ? UTF8_TO_TCHAR(Description) : FString();

			Out.bIsHidden = ReadHiddenAttribute(UserStats, AchievementId);

			bool bUnlocked = false;
			uint32 UnlockTime = 0;
			if (UserStats.GetAchievementAndUnlockTime(AchievementId, &bUnlocked, &UnlockTime))
			{
				Out.bIsUnlocked = bUnlocked;
				Out.UnlockTime = (bUnlocked && UnlockTime > 0)
					                 ? FDateTime::FromUnixTimestamp(static_cast<int64>(UnlockTime))
					                 : FDateTime();
			}

			// Only achievements configured with a progress stat have limits; the rest are binary.
			int32 MinProgress = 0;
			int32 MaxProgress = 0;
			if (UserStats.GetAchievementProgressLimits(AchievementId, &MinProgress, &MaxProgress))
			{
				Out.TargetValue = MaxProgress;
			}

			// Steam serves achievement icons as image handles (GetAchievementIcon + ISteamUtils), not
			// URLs, so IconUrl is left empty rather than filled with something that resolves to nothing.

			// Only meaningful once the global percentages have arrived; -1 otherwise, meaning "unknown".
			if (bGlobalPercentagesReady)
			{
				float Percent = 0.0f;
				if (UserStats.GetAchievementAchievedPercent(AchievementId, &Percent))
				{
					Out.GlobalUnlockPercent = Percent;
				}
			}
		}

		/**
		 * Recomputes Progress from the values Steam gave us. Unlocked is always 1.0; a locked
		 * achievement is only measurable once Steam has told us a current value (see the class
		 * comment), so it stays 0 until then rather than inventing a fraction.
		 */
		static void UpdateProgressFraction(FGameAchievement& Achievement)
		{
			if (Achievement.bIsUnlocked)
			{
				Achievement.Progress = 1.0;

				// Steam does not report a current value for an achievement that is already unlocked, so
				// without this a finished progressive achievement reads "0 / 100". EOS reports the two
				// as equal, and the common struct has to mean the same thing on both.
				if (Achievement.TargetValue > 0)
				{
					Achievement.CurrentValue = FMath::Max(Achievement.CurrentValue, Achievement.TargetValue);
				}
				return;
			}

			Achievement.Progress = (Achievement.TargetValue > 0)
				                       ? FMath::Clamp(static_cast<double>(Achievement.CurrentValue) /
				                                      static_cast<double>(Achievement.TargetValue), 0.0, 1.0)
				                       : 0.0;
		}

		/** Rebuilds the whole cache from the Steam client's copy of the player's state. */
		void RefreshCache()
		{
			ISteamUserStats* UserStats = ::SteamUserStats();
			if (!UserStats)
			{
				return;
			}

			// Steam does not report a locked achievement's current progress, so carry forward anything
			// UserAchievementStored_t has told us this session instead of resetting it to zero.
			TMap<FString, int32> KnownProgress;
			for (const FGameAchievement& Achievement : Cached)
			{
				if (!Achievement.bIsUnlocked && Achievement.CurrentValue > 0)
				{
					KnownProgress.Add(Achievement.Id, Achievement.CurrentValue);
				}
			}

			TArray<FGameAchievement> Refreshed;
			const uint32 Count = UserStats->GetNumAchievements();
			Refreshed.Reserve(Count);

			for (uint32 Index = 0; Index < Count; ++Index)
			{
				const char* AchievementId = UserStats->GetAchievementName(Index);
				if (!AchievementId)
				{
					continue;
				}

				FGameAchievement Achievement;
				ReadAchievement(*UserStats, AchievementId, Achievement);
				if (const int32* Known = KnownProgress.Find(Achievement.Id))
				{
					Achievement.CurrentValue = *Known;
				}
				UpdateProgressFraction(Achievement);
				Refreshed.Add(MoveTemp(Achievement));
			}

			Cached = MoveTemp(Refreshed);
		}

		/**
		 * Fires for both halves of Steam's achievement writes: a progress update carries the current
		 * and maximum values, while an actual unlock carries zero for both.
		 */
		void OnAchievementStored(UserAchievementStored_t* Param)
		{
			if (!Param)
			{
				return;
			}

			const FString AchievementId = UTF8_TO_TCHAR(Param->m_rgchAchievementName);
			FGameAchievement* Achievement = FindCached(AchievementId);
			if (!Achievement)
			{
				RefreshCache();
				Achievement = FindCached(AchievementId);
				if (!Achievement)
				{
					return;
				}
			}

			if (Param->m_nMaxProgress == 0)
			{
				const bool bWasUnlocked = Achievement->bIsUnlocked;
				Achievement->bIsUnlocked = true;
				Achievement->CurrentValue = Achievement->TargetValue;
				Achievement->UnlockTime = FDateTime::UtcNow();
				UpdateProgressFraction(*Achievement);

				if (!bWasUnlocked && Owner.OnAchievementUnlocked)
				{
					Owner.OnAchievementUnlocked(*Achievement);
				}
			}
			else
			{
				Achievement->CurrentValue = static_cast<int32>(Param->m_nCurProgress);
				Achievement->TargetValue = static_cast<int32>(Param->m_nMaxProgress);
				UpdateProgressFraction(*Achievement);
			}

			if (Owner.OnAchievementsChanged)
			{
				Owner.OnAchievementsChanged();
			}
		}

		/** Steam delivers the player's stats and achievements shortly after init, and on reconnects. */
		void OnUserStatsReceived(UserStatsReceived_t* Param)
		{
			if (!Param || Param->m_eResult != k_EResultOK)
			{
				return;
			}

			RefreshCache();
			if (Owner.OnAchievementsChanged)
			{
				Owner.OnAchievementsChanged();
			}
		}
	};

	FSteamAchievements::FSteamAchievements(FSteamPlatformCore& InCore)
		: Core(InCore)
		  , Impl(MakePimpl<FImpl>(*this))
	{
	}

	FSteamAchievements::~FSteamAchievements() = default;

	bool FSteamAchievements::IsReady() const
	{
		return Core.IsInitialized() && Core.IsLoggedIn() && ::SteamUserStats() != nullptr;
	}

	const TArray<FGameAchievement>& FSteamAchievements::GetCachedAchievements() const
	{
		return Impl->Cached;
	}

	void FSteamAchievements::UnlockAchievement(const FString& AchievementId,
	                                           TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamUserStats* UserStats = ::SteamUserStats();
		if (!ensureMsgf(IsReady() && UserStats,
		                TEXT("SteamworksGamingService: UnlockAchievement called when the service is not ready")))
		{
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Unlocking achievement: %s"), *AchievementId);

		const FTCHARToUTF8 AchievementIdUtf8(*AchievementId);
		const char* AchievementIdRaw = AchievementIdUtf8.Get();

		bool bAlreadyUnlocked = false;
		if (!UserStats->GetAchievement(AchievementIdRaw, &bAlreadyUnlocked))
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Achievement does not exist: %s"), *AchievementId);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		if (bAlreadyUnlocked)
		{
			// Unlocking an already-unlocked achievement is an idempotent success, matching EOS (whose
			// UnlockAchievements is idempotent). Reporting failure here would make re-unlocks and
			// re-runs spuriously fail and diverge from the EOS backend.
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Achievement already unlocked: %s"), *AchievementId);
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
			return;
		}

		const bool bSuccess = UserStats->SetAchievement(AchievementIdRaw);
		if (bSuccess)
		{
			// The unlock only reaches Steam's servers (and the player's toast) on StoreStats; the
			// UserAchievementStored_t that follows is what updates the cache and fires the sink.
			UserStats->StoreStats();
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Achievement unlocked successfully: %s"),
			       *AchievementId);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to unlock achievement: %s"), *AchievementId);
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void FSteamAchievements::QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback)
	{
		if (!ensureMsgf(IsReady(),
		                TEXT("SteamworksGamingService: QueryAchievements called when the service is not ready")))
		{
			if (Callback)
			{
				Callback(FAchievementsQueryResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Querying achievements..."));

		// Ask for the global unlock percentages once. They come back over the network, so this query
		// answers immediately from the client's own copy and rarity fills in on a later query — the
		// alternative is stalling every first query on a round trip for a cosmetic field.
		if (!Impl->bGlobalPercentagesRequested)
		{
			if (ISteamUserStats* UserStats = ::SteamUserStats())
			{
				const SteamAPICall_t Handle = UserStats->RequestGlobalAchievementPercentages();

				// Registering a call result for an invalid handle would wait on a call that never
				// completes, and leave the entry in the pump for the rest of the session. Leave the
				// request unflagged instead so a later query retries it.
				if (Handle == k_uAPICallInvalid)
				{
					UE_LOG(LogTemp, Log,
					       TEXT("SteamworksGamingService: RequestGlobalAchievementPercentages was refused; "
						       "GlobalUnlockPercent stays unknown for now"));
				}
				else
				{
					Impl->bGlobalPercentagesRequested = true;
					Core.GetCallResults().Add<GlobalAchievementPercentagesReady_t>(
						Handle,
						[this](const GlobalAchievementPercentagesReady_t& Result, bool bIOFailure)
						{
							if (bIOFailure || Result.m_eResult != k_EResultOK)
							{
								UE_LOG(LogTemp, Log,
								       TEXT("SteamworksGamingService: global achievement percentages "
									       "unavailable; GlobalUnlockPercent stays unknown"));
								return;
							}

							Impl->bGlobalPercentagesReady = true;
							Impl->RefreshCache();
							if (OnAchievementsChanged)
							{
								OnAchievementsChanged();
							}
						});
				}
			}
		}

		Impl->RefreshCache();

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Queried %d achievements"), Impl->Cached.Num());

		if (Callback)
		{
			Callback(FAchievementsQueryResult(true, Impl->Cached));
		}
	}

	void FSteamAchievements::IndicateProgress(const FString& AchievementId, int32 CurrentValue, int32 MaxValue,
	                                          TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamUserStats* UserStats = ::SteamUserStats();
		if (!IsReady() || !UserStats || MaxValue <= 0)
		{
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		const FTCHARToUTF8 AchievementIdUtf8(*AchievementId);
		const bool bSuccess = UserStats->IndicateAchievementProgress(
			AchievementIdUtf8.Get(), static_cast<uint32>(FMath::Max(0, CurrentValue)),
			static_cast<uint32>(MaxValue));

		if (!bSuccess)
		{
			UE_LOG(LogTemp, Verbose,
			       TEXT("SteamworksGamingService: IndicateAchievementProgress refused for %s (%d/%d) — usually an "
				       "achievement with no progress stat, or one that is already unlocked"),
			       *AchievementId, CurrentValue, MaxValue);
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void FSteamAchievements::ResetAchievements(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamUserStats* UserStats = ::SteamUserStats();
		if (!IsReady() || !UserStats)
		{
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Warning,
		       TEXT("SteamworksGamingService: resetting ALL achievements and stats for this user"));

		// The parameter here is Steam's "achievements too" — stats are cleared either way.
		const bool bSuccess = UserStats->ResetAllStats(true);
		if (bSuccess)
		{
			UserStats->StoreStats();

			// Drop the cache outright rather than refreshing it. RefreshCache deliberately carries
			// forward the progress values Steam only reports once (see its comment), and after a wipe
			// those are exactly the numbers that must not survive.
			Impl->Cached.Reset();
			Impl->RefreshCache();
			if (OnAchievementsChanged)
			{
				OnAchievementsChanged();
			}
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}
}

#endif // GS_WITH_STEAM

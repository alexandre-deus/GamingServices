#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamAchievements.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	void FSteamAchievements::UnlockAchievement(const FString& AchievementId, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: UnlockAchievement called when "
					"service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Unlocking achievement: %s"), *AchievementId);

		FTCHARToUTF8 UTF8String(*AchievementId);
		const char* AchievementIdUTF8 = UTF8String.Get();

		bool bAchievementExists = SteamUserStats->GetAchievement(AchievementIdUTF8, nullptr);
		if (!bAchievementExists)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Achievement does not exist: %s"), *AchievementId);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		bool bAlreadyUnlocked = false;
		SteamUserStats->GetAchievement(AchievementIdUTF8, &bAlreadyUnlocked);
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

		bool bSuccess = SteamUserStats->SetAchievement(AchievementIdUTF8);

		if (bSuccess)
		{
			SteamUserStats->StoreStats();
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
		ISteamUserStats* SteamUserStats = ::SteamUserStats();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamUserStats,
			   TEXT("SteamworksGamingService: QueryAchievements called when "
					"service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Querying achievements..."));

		TArray<FGameAchievement> Achievements;

		uint32 AchievementCount = SteamUserStats->GetNumAchievements();

		for (uint32 i = 0; i < AchievementCount; ++i)
		{
			FGameAchievement GameAchievement;

			const char* AchievementId = SteamUserStats->GetAchievementName(i);
			if (AchievementId)
			{
				GameAchievement.Id = UTF8_TO_TCHAR(AchievementId);

				const char* AchievementDisplayName =
					SteamUserStats->GetAchievementDisplayAttribute(AchievementId, "name");
				GameAchievement.DisplayName =
					AchievementDisplayName ? UTF8_TO_TCHAR(AchievementDisplayName) : GameAchievement.Id;

				const char* Description = SteamUserStats->GetAchievementDisplayAttribute(AchievementId, "desc");
				GameAchievement.Description = Description ? UTF8_TO_TCHAR(Description) : TEXT("");

				bool bUnlocked = false;
				SteamUserStats->GetAchievement(AchievementId, &bUnlocked);
				GameAchievement.bIsUnlocked = bUnlocked;

				float Progress = 0.0f;
				SteamUserStats->GetAchievementAchievedPercent(AchievementId, &Progress);
				GameAchievement.Progress = Progress;

				Achievements.Add(GameAchievement);
			}
		}

		if (Callback)
		{
			Callback(FAchievementsQueryResult(true, Achievements));
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Queried %d achievements"), Achievements.Num());
	}
}

#endif // GS_WITH_STEAM

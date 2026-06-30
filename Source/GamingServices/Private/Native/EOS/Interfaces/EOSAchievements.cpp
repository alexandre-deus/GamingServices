#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSAchievements.h"
#include "Native/EOS/EOSPlatformCore.h"
#include "EOSCallbackContext.h"

namespace GamingServices
{
	using FAchievementUnlockCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSAchievements>;
	using FAchievementsQueryCallbackCtx = TEOSCallbackContext<FAchievementsQueryResult, FEOSAchievements>;

	// Cast the core's opaque accessors back to their EOS_* types in this .cpp so the core header stays SDK-free.
	static EOS_HAchievements AchievementsHandle(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_HAchievements>(Core.GetAchievementsHandle());
	}

	static EOS_ProductUserId ProductUserId(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_ProductUserId>(Core.GetProductUserId());
	}

	void FEOSAchievements::UnlockAchievement(const FString& AchievementId,
	                                         TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetAchievementsHandle(),
		       TEXT("EOSGamingService: UnlockAchievement called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Unlocking achievement: %s"), *AchievementId);

		EOS_Achievements_UnlockAchievementsOptions UnlockOptions = {};
		UnlockOptions.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST;
		UnlockOptions.UserId = ProductUserId(Core);

		const char* AchievementIds[] = {TCHAR_TO_UTF8(*AchievementId)};
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
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetAchievementsHandle(),
		       TEXT("EOSGamingService: QueryAchievements called when service not ready"));

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

				TArray<FGameAchievement> Achievements;
				for (const void* DefinitionPtr : Self->Core.GetAchievementDefinitionPtrs())
				{
					const EOS_Achievements_DefinitionV2* Definition =
						static_cast<const EOS_Achievements_DefinitionV2*>(DefinitionPtr);
					if (Definition)
					{
						FGameAchievement GameAchievement;
						ConvertEOSAchievementToGameAchievement(Definition, nullptr, GameAchievement);
						Achievements.Add(GameAchievement);
					}
				}

				Result.Achievements = Achievements;
				FAchievementsQueryCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}
}

#endif // USE_EOS

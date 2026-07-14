#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "DataTypes/AchievementTypes.h"
#include "DataTypes/LeaderboardTypes.h"
#include "DataTypes/SessionTypes.h"

#include "eos_sdk.h"
#include "eos_common.h"
#include "eos_auth.h"
#include "eos_achievements.h"
#include "eos_stats.h"
#include "eos_leaderboards.h"
#include "eos_connect.h"
#include "eos_logging.h"
#include "eos_playerdatastorage.h"
#include "eos_lobby.h"
#include "eos_ecom.h"
#include "eos_userinfo.h"

namespace GamingServices
{
	/**
	 * Heap-allocated context threaded through EOS SDK callbacks as ClientData.
	 *
	 * Mirrors the legacy TEOSCallbackContext: it carries a typed result callback plus a back-pointer to
	 * the issuing object (here a capability class instead of the old fat service), and self-deletes once
	 * the callback runs. TOwner is the capability class that owns the in-flight request, so SDK callbacks
	 * can reach the platform core via Ctx->Service->Core.
	 */
	template <typename TResult, typename TOwner>
	struct TEOSCallbackContext
	{
		TOwner* Service;
		TFunction<void(const TResult&)> Callback;

		static TEOSCallbackContext* Create(TOwner* InService, TFunction<void(const TResult&)> InCallback)
		{
			TEOSCallbackContext* Ctx = new TEOSCallbackContext{};
			Ctx->Service = InService;
			Ctx->Callback = MoveTemp(InCallback);
			return Ctx;
		}

		static void Complete(TEOSCallbackContext* Ctx, const TResult& Result)
		{
			if (Ctx->Callback)
			{
				Ctx->Callback(Result);
			}
			delete Ctx;
		}
	};

	/**
	 * Backend handle for a lobby discovered through FindSessions or a lobby invite. Owns the
	 * EOS_HLobbyDetails handle and releases it on destruction.
	 *
	 * Lives in private infra (this header) rather than the SDK-free EOSPlatformCore.h so the core header
	 * does not leak the EOS SDK. The matchmaking capability and the lobby-invite notification create it.
	 */
	struct FEOSSessionJoinHandle : public ISessionJoinHandle
	{
		EOS_HLobbyDetails Handle = nullptr;
		FString LobbyId;
		FString SessionName;

		FEOSSessionJoinHandle(EOS_HLobbyDetails InHandle, const FString& InLobbyId, const FString& InSessionName)
			: Handle(InHandle), LobbyId(InLobbyId), SessionName(InSessionName) {}

		~FEOSSessionJoinHandle()
		{
			if (Handle)
			{
				EOS_LobbyDetails_Release(Handle);
				Handle = nullptr;
			}
		}
	};

	// Conversion helpers reused by the achievements / leaderboards capabilities. These take EOS SDK types,
	// so they live in private infra rather than the SDK-free core header.
	inline void ConvertEOSAchievementToGameAchievement(const EOS_Achievements_DefinitionV2* EOSDefinition,
	                                                    const EOS_Achievements_PlayerAchievement* EOSPlayerAchievement,
	                                                    FGameAchievement& GameAchievement)
	{
		if (EOSDefinition)
		{
			GameAchievement.Id = UTF8_TO_TCHAR(EOSDefinition->AchievementId);
			GameAchievement.DisplayName = UTF8_TO_TCHAR(EOSDefinition->UnlockedDisplayName);
			GameAchievement.Description = UTF8_TO_TCHAR(EOSDefinition->UnlockedDescription);
		}

		if (EOSPlayerAchievement)
		{
			// Locked achievements report EOS_ACHIEVEMENTS_ACHIEVEMENT_UNLOCKTIME_UNDEFINED (-1), so a
			// plain "!= 0" check would read locked as unlocked.
			GameAchievement.bIsUnlocked =
				(EOSPlayerAchievement->UnlockTime != EOS_ACHIEVEMENTS_ACHIEVEMENT_UNLOCKTIME_UNDEFINED);
			GameAchievement.Progress = EOSPlayerAchievement->Progress;
		}
	}

	inline void ConvertEOSLeaderboardRecordToEntry(const EOS_Leaderboards_LeaderboardRecord* EOSRecord,
	                                               FLeaderboardEntry& Entry)
	{
		if (EOSRecord)
		{
			// Record->UserId is an EOS_ProductUserId handle, not a string; stringify it properly.
			char PuidStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
			int32_t PuidLen = sizeof(PuidStr);
			if (EOS_ProductUserId_ToString(EOSRecord->UserId, PuidStr, &PuidLen) == EOS_EResult::EOS_Success)
			{
				Entry.UserId = UTF8_TO_TCHAR(PuidStr);
			}
			Entry.DisplayName = UTF8_TO_TCHAR(EOSRecord->UserDisplayName);
			Entry.Score = EOSRecord->Score;
			Entry.Rank = EOSRecord->Rank;
		}
	}
}

#endif // USE_EOS

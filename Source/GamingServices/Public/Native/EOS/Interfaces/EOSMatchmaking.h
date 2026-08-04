#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IMatchmakingService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/**
	 * EOS matchmaking capability built on the Lobby API (not Sessions: lobbies push member join/leave
	 * notifications and carry per-member attributes, giving feature parity with the Steam backend).
	 *
	 * The "session" the IMatchmakingService contract speaks of maps to an EOS lobby. The human-facing
	 * session name travels as an advertised lobby attribute; each member advertises their display name
	 * as a member attribute so the join/leave sinks can carry real names.
	 *
	 * Also owns the lobby notifications (invite-accepted + member-status). Their registration needs the
	 * lobby handle and ProductUserId that only exist after login, so the constructor binds the core's
	 * Register/UnregisterMatchmakingNotificationsHook; the core fires those at the correct points in the
	 * login / shutdown sequence and this class does the EOS_Lobby_AddNotify* / RemoveNotify* work,
	 * firing its own sinks directly.
	 */
	class FEOSMatchmaking final : public IMatchmakingService
	{
	public:
		explicit FEOSMatchmaking(FEOSPlatformCore& InCore);

		virtual void CreateSession(const FSessionSettings& Settings,
		                           TFunction<void(const FSessionCreateResult&)> Callback) override;
		virtual void FindSessions(const FSessionSearchFilter& Filter,
		                          TFunction<void(const FSessionSearchResult&)> Callback) override;
		virtual void JoinSession(const FSessionJoinHandle& JoinHandle,
		                         TFunction<void(const FSessionJoinResult&)> Callback) override;
		virtual void LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void DestroySession(TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void UpdateSession(const FSessionSettings& Settings,
		                           TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void LockLobby(TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback) override;
		virtual void ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback) override;

		virtual void JoinLobbyById(const FString& LobbyId,
		                           TFunction<void(const FSessionJoinResult&)> Callback) override;
		virtual FString GetCurrentLobbyId() const override { return CurrentLobbyId; }

		virtual FString GetSessionConnectionString() const override;

		// True on desktop, where the EOS Social Overlay shows invites; false on mobile, which has no
		// overlay. See the implementation for why this is a platform split rather than a fixed answer.
		virtual bool PlatformOwnsInviteUI() const override;

		virtual void RejectInvite(const FString& InviteId,
		                          TFunction<void(const FGamingServiceResult&)> Callback) override;

		virtual void QueryPendingInvites(TFunction<void(const FPendingInvitesResult&)> Callback) override;

	private:
		// Applies Settings onto the current lobby via EOS_Lobby_UpdateLobbyModification + UpdateLobby.
		// Shared by UpdateSession / LockLobby / UnlockLobby (and CreateSession's post-create attribute pass).
		void ApplySessionSettings(const FSessionSettings& Settings, const TCHAR* OperationName,
		                          const TCHAR* SuccessMessage,
		                          TFunction<void(const FGamingServiceResult&)> Callback);

		// Lobby notifications, registered / torn down from the core's login / shutdown hooks.
		void RegisterLobbyNotifications();
		void UnregisterLobbyNotifications();

		void ResetLobbyState();

		/**
		 * Shared tail of both join paths (details-handle join and join-by-id): caches the lobby state from
		 * the joined lobby, fills OutResult, and advertises this member's display name so the host's join
		 * notification carries a real name.
		 *
		 * LobbyDetails is an EOS_HLobbyDetails passed as void* to keep this header SDK-free, and stays
		 * owned by the caller.
		 */
		void FinalizeJoinedLobby(void* LobbyDetails, const FString& LobbyId, const FString& SessionName,
		                         FSessionJoinResult& OutResult);

		FEOSPlatformCore& Core;

		// Lobby membership bookkeeping ("session" state as seen through IMatchmakingService).
		bool bIsInLobby = false;
		bool bIsLobbyOwner = false;
		FString CurrentLobbyId;
		FString CurrentLobbyOwnerPuid;
		FString CurrentSessionName;
		FSessionSettings CurrentSessionSettings;

		// EOS_NotificationId (uint64); 0 == EOS_INVALID_NOTIFICATIONID. Stored as uint64 so this header
		// stays SDK-free.
		uint64 LobbyInviteAcceptedNotificationId = 0;
		uint64 LobbyInviteReceivedNotificationId = 0;
		uint64 LobbyMemberStatusNotificationId = 0;
	};
}

#endif // GS_WITH_EOS

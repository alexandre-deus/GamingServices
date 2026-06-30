#pragma once

#if defined(USE_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IMatchmakingService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/**
	 * EOS matchmaking capability (Sessions API) over the shared platform core.
	 *
	 * Also owns the session-invite-accepted notification. Its registration needs the sessions handle and
	 * ProductUserId that only exist after login, so the constructor binds the core's
	 * Register/UnregisterSessionInviteNotificationHook; the core fires those at the correct points in the
	 * login / shutdown sequence and this class does the EOS_Sessions_AddNotifySessionInviteAccepted /
	 * Remove work, firing its own OnLobbyInviteAccepted sink directly.
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

		virtual FString GetSessionConnectionString() const override;

	private:
		void ApplySessionSettings(const FSessionSettings& Settings, const TCHAR* OperationName,
		                          const TCHAR* SuccessMessage,
		                          TFunction<void(const FGamingServiceResult&)> Callback);

		// Session-invite-accepted notification, fired from the core's login / shutdown hooks.
		void RegisterSessionInviteNotification();
		void UnregisterSessionInviteNotification();

		FEOSPlatformCore& Core;

		// EOS_NotificationId (uint64); 0 == EOS_INVALID_NOTIFICATIONID. Stored as uint64 so this header
		// stays SDK-free.
		uint64 SessionInviteAcceptedNotificationId = 0;
	};
}

#endif // USE_EOS

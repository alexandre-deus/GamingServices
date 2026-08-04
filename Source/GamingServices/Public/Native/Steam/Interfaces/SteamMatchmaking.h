#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IMatchmakingService.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	class FSteamPlatformCore;
	class FSteamUser;

	/**
	 * Steam lobby-based session ops.
	 *
	 * Identity/lifecycle comes from FSteamPlatformCore; async ops register on its CallResults pump.
	 * All lobby state, the lobby CCallbacks (LobbyChatUpdate_t / GameLobbyJoinRequested_t), the
	 * lobby-search machinery, and the Steam session join-handle live in a private FImpl (pimpl) so
	 * this header stays free of the Steam SDK. The lobby callbacks fire this object's own inherited
	 * notification sinks directly. Avatar fetches for lobby members are delegated to FSteamUser.
	 */
	class FSteamMatchmaking final : public IMatchmakingService
	{
	public:
		FSteamMatchmaking(FSteamPlatformCore& InCore, FSteamUser& InUser);

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
		virtual FString GetCurrentLobbyId() const override;

		virtual FString GetSessionConnectionString() const override;

		// The Steam overlay shows the invite and takes the accept / decline. The game never sees the invite
		// before that decision, so OnLobbyInviteReceived never fires here and drawing a toast would double
		// up with the overlay the player is already using.
		virtual bool PlatformOwnsInviteUI() const override;

		// Nothing to reject: by the time Steam tells us anything, the player already chose in the overlay.
		virtual void RejectInvite(const FString& InviteId,
		                          TFunction<void(const FGamingServiceResult&)> Callback) override;

		// Steam keeps pending invites in the overlay and the friends list, not somewhere the game can read.
		virtual void QueryPendingInvites(TFunction<void(const FPendingInvitesResult&)> Callback) override;

		/** Drives pending lobby-search contexts; called each frame by the owning service. */
		void Tick();

	private:
		struct FImpl;

		FSteamPlatformCore& Core;
		FSteamUser& User;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_STEAM

#pragma once

#include "CoreMinimal.h"
#include "Native/Interfaces/IMatchmakingService.h"

class IInviteTransport;

namespace GamingServices
{
	/**
	 * Matchmaking for a multi-backend arrangement: the session stays entirely on the primary backend,
	 * while invites travel over the identity backend's platform.
	 *
	 * Everything here forwards to the primary's matchmaking — this deliberately owns no session state of
	 * its own, so there is no second source of truth to drift. What it adds is the wiring between the two
	 * backends:
	 *
	 *   - after create/join, the primary's lobby id is published to the invite transport, so friends on
	 *     the identity platform see a joinable game and can be invited to it;
	 *   - after leave/destroy, it is withdrawn;
	 *   - ShowInviteFriendsDialog opens the identity platform's own friend picker, because that is where
	 *     the player's friends actually are (the primary has no social graph under external auth);
	 *   - an accepted invite comes back as a bare lobby id, which is surfaced through the ordinary
	 *     OnLobbyInviteAccepted sink as an id-only join handle. Callers cannot tell it apart from a
	 *     native invite, so the game's existing invite-accept flow needs no changes.
	 */
	class GAMINGSERVICES_API FCompositeMatchmaking final : public IMatchmakingService
	{
	public:
		FCompositeMatchmaking(IMatchmakingService& InInner, IInviteTransport* InInviteTransport);
		virtual ~FCompositeMatchmaking() override;

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

		virtual bool PlatformOwnsInviteUI() const override;
		virtual void RejectInvite(const FString& InviteId,
		                          TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void QueryPendingInvites(TFunction<void(const FPendingInvitesResult&)> Callback) override;

	private:
		/** Publishes the primary's current lobby id to the transport, or withdraws it when there is none. */
		void PublishJoinInfo();
		void WithdrawJoinInfo();

		IMatchmakingService& Inner;
		IInviteTransport* InviteTransport = nullptr;
	};
}

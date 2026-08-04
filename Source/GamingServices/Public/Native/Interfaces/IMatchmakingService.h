#pragma once

#include "CoreMinimal.h"
#include "DataTypes/SessionTypes.h"
#include "Native/GamingCapability.h"

/**
 * Session / lobby matchmaking capability: create, find, join, leave, destroy, update, lock,
 * invite, plus host-side membership notifications.
 *
 * The On* members are notification sinks set by the owner (e.g. the subsystem) and invoked by the
 * backend when the platform reports the corresponding event.
 */
class GAMINGSERVICES_API IMatchmakingService
{
public:
	virtual ~IMatchmakingService() = default;

	virtual void CreateSession(const FSessionSettings& Settings,
	                           TFunction<void(const FSessionCreateResult&)> Callback) = 0;
	virtual void FindSessions(const FSessionSearchFilter& Filter,
	                          TFunction<void(const FSessionSearchResult&)> Callback) = 0;
	virtual void JoinSession(const FSessionJoinHandle& JoinHandle,
	                         TFunction<void(const FSessionJoinResult&)> Callback) = 0;
	virtual void LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void DestroySession(TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void UpdateSession(const FSessionSettings& Settings,
	                           TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void LockLobby(TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback) = 0;
	virtual void ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	// Join a lobby directly by its backend id string — no search bucket, no presence, no EAS friends.
	// Lets a host share a "join code" (the lobby id) out of band and a peer join with just that string.
	virtual void JoinLobbyById(const FString& LobbyId,
	                           TFunction<void(const FSessionJoinResult&)> Callback) = 0;

	// Backend id of the lobby the local user is currently in (empty when not in one) — the string a host
	// shares as a join code for JoinLobbyById.
	virtual FString GetCurrentLobbyId() const = 0;

	virtual FString GetSessionConnectionString() const = 0;

	/**
	 * Whether the platform presents its own accept / decline UI for incoming invites.
	 *
	 * True means the player decides inside an overlay the game does not draw and cannot intercept: only
	 * OnLobbyInviteAccepted will ever fire, OnLobbyInviteReceived stays silent, and drawing an in-game
	 * toast would duplicate UI the player is already looking at. False means nothing will show an invite
	 * unless the game does.
	 *
	 * Runtime state, not a compile-time property: a build that normally defers to an overlay still has to
	 * draw its own UI when that platform turns out to be unavailable.
	 */
	virtual bool PlatformOwnsInviteUI() const = 0;

	/**
	 * Turn down an invite that was delivered through OnLobbyInviteReceived, so it stops being pending and
	 * the sender is told. Backends whose invites are owned by a platform overlay have nothing to reject —
	 * the overlay already consumed the decision — and report failure.
	 */
	virtual void RejectInvite(const FString& InviteId,
	                          TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	/**
	 * Ask the backend which invites are already waiting, rather than waiting to be told about new ones.
	 *
	 * OnLobbyInviteReceived only covers invites that arrive while the game is running and listening, so
	 * anything sent before launch, or during sign-in, is invisible to it. Poll after login to pick those
	 * up. Backends where an overlay owns invites report failure with an empty list: the invites exist, but
	 * they belong to a UI this game does not drive.
	 */
	virtual void QueryPendingInvites(TFunction<void(const FPendingInvitesResult&)> Callback) = 0;

	// Notification sinks (set by owner, fired by backend).
	TFunction<void(const FSessionMemberInfo&)> OnSessionUserJoined;
	TFunction<void(const FSessionMemberInfo&)> OnSessionUserLeft;
	TFunction<void(const FGamingServiceResult&)> OnSessionEnded;
	TFunction<void(const FLobbyInviteAcceptedInfo&)> OnLobbyInviteAccepted;

	/**
	 * An invite arrived and nobody has decided about it yet — the game owns the accept / decline UI.
	 *
	 * Only fires on backends that surface invites before acceptance. Where a platform overlay owns that
	 * step (Steam), this stays silent and OnLobbyInviteAccepted is the only thing that ever fires, so a
	 * listener must not treat "never called" as an error.
	 */
	TFunction<void(const FLobbyInviteReceivedInfo&)> OnLobbyInviteReceived;
};

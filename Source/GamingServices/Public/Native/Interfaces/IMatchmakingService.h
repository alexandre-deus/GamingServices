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

	// Notification sinks (set by owner, fired by backend).
	TFunction<void(const FSessionMemberInfo&)> OnSessionUserJoined;
	TFunction<void(const FSessionMemberInfo&)> OnSessionUserLeft;
	TFunction<void(const FGamingServiceResult&)> OnSessionEnded;
	TFunction<void(const FLobbyInviteAcceptedInfo&)> OnLobbyInviteAccepted;
};

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/FriendTypes.h"
#include "DataTypes/GamingServiceResult.h"

/**
 * The local user's social graph: read the friend list, and invite a specific friend to the current
 * session.
 *
 * This is the targeted counterpart to IMatchmakingService::ShowInviteFriendsDialog and
 * IInviteTransport::ShowInviteDialog, which open the platform's own overlay picker. Prefer those when a
 * native overlay exists — they are the flow players already know and they enforce the player's privacy
 * and blocklist settings for you. This interface exists for the cases the overlay cannot serve: a
 * bespoke in-game friend list, platforms with no usable overlay, and anything that needs the names and
 * presence states as data rather than as someone else's UI.
 *
 * Availability differs sharply per backend, so treat GetFriends() being null as normal:
 *   - Steam reads ISteamFriends, which is populated as soon as Steam is running.
 *   - EOS reads the Friends interface, which is part of Epic Account Services and therefore needs an
 *     EpicAccountId from EOS_Auth_Login. A user who signed in through EOS_Connect only (the Steam-into-
 *     EOS arrangement) has no EpicAccountId at all, so EOS reports no friends capability there and the
 *     social graph must come from the identity backend instead.
 */
class GAMINGSERVICES_API IFriendsService
{
public:
	virtual ~IFriendsService() = default;

	/** Whether the platform can serve friend data right now (signed in, SDK up, graph readable). */
	virtual bool IsAvailable() const = 0;

	/**
	 * Fetch the friend list. The callback fires exactly once.
	 *
	 * Names are resolved as far as the platform's cache allows; any friend still unresolved comes back
	 * with DisplayName set to UserId rather than blank, so the list is always displayable. Bind
	 * OnFriendsChanged to refresh when late names or presence updates land.
	 */
	virtual void QueryFriends(TFunction<void(const FQueryFriendsResult&)> Callback) = 0;

	/**
	 * Last successfully queried list, without re-querying. Empty before the first QueryFriends.
	 * Cheap enough to call from UI tick or a list refresh.
	 */
	virtual const TArray<FGamingFriend>& GetCachedFriends() const = 0;

	/**
	 * Invite one friend to the session currently published for invites.
	 *
	 * Requires something to invite TO: the session backend must have an active session and, where the
	 * social platform is not the session platform, IInviteTransport::SetJoinInfo must already have
	 * published the payload. Fails rather than silently doing nothing when nothing is joinable.
	 *
	 * Fails for an offline friend, and may fail for reasons the caller cannot see or fix — the friend's
	 * privacy settings, a full session, or the platform rate-limiting invites. Surface it, do not retry.
	 */
	virtual void SendInvite(const FString& FriendUserId,
	                        TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	/**
	 * Fires when the platform reports the graph has changed — a friend came online, changed presence, or
	 * a name finished downloading. Carries no payload: re-read GetCachedFriends() or re-query.
	 */
	TFunction<void()> OnFriendsChanged;
};

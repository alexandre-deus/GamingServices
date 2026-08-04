#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IFriendsService.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	class FSteamPlatformCore;
	class FSteamInviteTransport;

	/**
	 * Steam social graph, read through ISteamFriends.
	 *
	 * Enumeration is synchronous — Steam keeps the friend list in the client — so QueryFriends completes
	 * on the calling frame. Presence and persona names may still be filling in behind it, which arrives as
	 * PersonaStateChange_t and fires OnFriendsChanged; a UI that re-reads GetCachedFriends() on that event
	 * converges without polling.
	 *
	 * Invites reuse the rich-presence connect string that FSteamInviteTransport publishes, so a targeted
	 * invite and the overlay's own invite button send the same thing. Steam requires no session of its own.
	 *
	 * The Steam SDK and its callbacks live in a private FImpl so this header stays SDK-free.
	 */
	class FSteamFriends final : public IFriendsService
	{
	public:
		FSteamFriends(FSteamPlatformCore& InCore, FSteamInviteTransport& InInviteTransport);
		virtual ~FSteamFriends() override;

		virtual bool IsAvailable() const override;
		virtual void QueryFriends(TFunction<void(const FQueryFriendsResult&)> Callback) override;
		virtual const TArray<FGamingFriend>& GetCachedFriends() const override;
		virtual void SendInvite(const FString& FriendUserId,
		                        TFunction<void(const FGamingServiceResult&)> Callback) override;

	private:
		struct FImpl;

		FSteamPlatformCore& Core;
		FSteamInviteTransport& InviteTransport;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_STEAM

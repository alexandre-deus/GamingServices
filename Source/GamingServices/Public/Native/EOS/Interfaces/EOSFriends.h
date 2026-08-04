#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/IFriendsService.h"
#include "Templates/PimplPtr.h"

class IMatchmakingService;

namespace GamingServices
{
	class FEOSPlatformCore;

	/**
	 * EOS social graph, read through the Friends and Presence interfaces.
	 *
	 * Both are Epic Account Services, so everything here needs an EpicAccountId from EOS_Auth_Login.
	 * FEOSGamingService therefore only hands this capability out once one exists — a Connect-only sign-in
	 * (Steam authenticating into EOS) reports no friends capability at all, because there is genuinely no
	 * Epic social graph to read for that user.
	 *
	 * Two EOS quirks shape this class:
	 *
	 *   - The friend list and presence are separate interfaces. EOS_Friends_GetStatus reports FRIENDSHIP
	 *     (friends / invite pending), not whether anyone is online, so online state is a second per-friend
	 *     query against Presence. Names are a third, against UserInfo. QueryFriends fans all of those out
	 *     and completes once the last one lands.
	 *
	 *   - Friends are EpicAccountIds but lobby invites take ProductUserIds. SendInvite therefore maps the
	 *     one to the other through the Connect external-account mappings before it can invite anybody.
	 *
	 * The EOS SDK lives in a private FImpl so this header stays SDK-free.
	 */
	class FEOSFriends final : public IFriendsService
	{
	public:
		FEOSFriends(FEOSPlatformCore& InCore, IMatchmakingService& InMatchmaking);
		virtual ~FEOSFriends() override;

		virtual bool IsAvailable() const override;
		virtual void QueryFriends(TFunction<void(const FQueryFriendsResult&)> Callback) override;
		virtual const TArray<FGamingFriend>& GetCachedFriends() const override;
		virtual void SendInvite(const FString& FriendUserId,
		                        TFunction<void(const FGamingServiceResult&)> Callback) override;

	private:
		struct FImpl;

		FEOSPlatformCore& Core;
		IMatchmakingService& Matchmaking;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_EOS

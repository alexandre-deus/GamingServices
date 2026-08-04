#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "FriendTypes.generated.h"

/**
 * Presence, flattened to the states every backend can actually report.
 *
 * Steam has a richer set (snooze, looking to trade/play) and EOS has fewer; both collapse into these.
 * Anything a backend cannot distinguish becomes Online, so a friend is never wrongly shown as Offline.
 */
UENUM(BlueprintType)
enum class EGamingFriendState : uint8
{
	Offline,
	Online,
	Away,
	Busy
};

/** One entry in the local user's friend list. */
USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FGamingFriend
{
	GENERATED_BODY()

	/**
	 * Backend-native id, in the same id space as IUserService::GetUserId on the backend that produced
	 * this list — so it can be passed straight back to SendInvite or ResolveDisplayName.
	 *
	 * Note this is the SOCIAL backend's id. Under a Steam-authenticates/EOS-hosts arrangement these are
	 * SteamIDs while the session is an EOS lobby, which is exactly why invites go out through
	 * IInviteTransport (it carries an opaque payload) rather than through the session backend.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Friends")
	FString UserId;

	/** Always non-empty: falls back to UserId when the platform has not cached a name yet. */
	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Friends")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Friends")
	EGamingFriendState State = EGamingFriendState::Offline;

	/**
	 * Whether the friend is running THIS game right now. Invites can be sent to anyone online, but these
	 * are the ones who can act on one immediately, so UI usually sorts them first.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Friends")
	bool bPlayingThisGame = false;

	bool IsOnline() const { return State != EGamingFriendState::Offline; }
};

/** Result of IFriendsService::QueryFriends. Friends is empty on failure. */
USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FQueryFriendsResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GamingServices|Friends")
	TArray<FGamingFriend> Friends;

	FQueryFriendsResult() = default;

	static FQueryFriendsResult Succeeded(TArray<FGamingFriend>&& InFriends)
	{
		FQueryFriendsResult R;
		R.bSuccess = true;
		R.Friends = MoveTemp(InFriends);
		return R;
	}

	static FQueryFriendsResult Failed()
	{
		return FQueryFriendsResult();
	}
};

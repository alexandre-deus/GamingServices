#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/FriendTypes.h"
#include "DataTypes/GamingServiceResult.h"
#include "FriendsLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQueryFriendsResultPin, const FQueryFriendsResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSendInviteResultPin, const FGamingServiceResult&, Result);

/**
 * Read the local user's friend list.
 *
 * Completes with bSuccess=false and an empty list when the platform has no readable social graph — most
 * commonly an EOS user who signed in through Connect rather than Auth, who has no EpicAccountId and
 * therefore no Epic friends. Branch on Result.bSuccess; do not treat "no friends" as an error.
 */
UCLASS()
class GAMINGSERVICES_API UAsyncAction_QueryFriends : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FQueryFriendsResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Friends", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_QueryFriends* QueryFriends(UObject* WorldContextObject);

	virtual void Activate() override;
};

/**
 * Invite one friend to the session that is currently published for invites.
 *
 * Requires something joinable to exist first: a lobby on the session backend, and — where the social
 * platform is not the session platform — a payload already published via the invite transport. Fails
 * rather than silently doing nothing when there is nothing to join.
 */
UCLASS()
class GAMINGSERVICES_API UAsyncAction_SendFriendInvite : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FSendInviteResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Friends", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_SendFriendInvite* SendFriendInvite(UObject* WorldContextObject, const FString& FriendUserId);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString FriendUserId;
};

/** Synchronous friend getters (no platform round-trip). */
UCLASS()
class GAMINGSERVICES_API UFriendsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Whether a friend list can be read at all on this platform, for this user, right now. False for a
	 * Connect-only EOS sign-in and before login. Use it to hide a friends UI rather than showing one that
	 * can only ever be empty.
	 */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Friends", meta = (WorldContext = "WorldContextObject"))
	static bool IsFriendsAvailable(const UObject* WorldContextObject);

	/** Last queried list, without re-querying. Empty before the first QueryFriends. */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Friends", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGamingFriend> GetCachedFriends(const UObject* WorldContextObject);

	/**
	 * Typed count / element accessors for a friend array.
	 *
	 * The generic Array Length and Array Get nodes would do this, but their wildcard pins have to be
	 * resolved by the editor from what you connect. These have concrete pins instead, which makes an
	 * iterating Blueprint graph safe to author programmatically and unambiguous to read.
	 */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Friends")
	static int32 GetFriendCount(const TArray<FGamingFriend>& Friends);

	/** Returns a default-constructed friend when Index is out of range, so callers cannot read past the end. */
	UFUNCTION(BlueprintPure, Category = "GamingServices|Friends")
	static FGamingFriend GetFriendAt(const TArray<FGamingFriend>& Friends, int32 Index);
};

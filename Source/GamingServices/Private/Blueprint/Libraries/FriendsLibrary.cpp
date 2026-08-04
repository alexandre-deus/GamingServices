#include "Blueprint/Libraries/FriendsLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IFriendsService.h"
#include "Blueprint/GamingPlatformSubsystem.h"

UAsyncAction_QueryFriends* UAsyncAction_QueryFriends::QueryFriends(UObject* WorldContextObject)
{
	UAsyncAction_QueryFriends* Action = NewObject<UAsyncAction_QueryFriends>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_QueryFriends::Activate()
{
	IGamingService* Service = ResolveService();
	IFriendsService* Friends = Service ? Service->GetFriends() : nullptr;
	if (!Friends)
	{
		Completed.Broadcast(FQueryFriendsResult::Failed());
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Friends->QueryFriends([this](const FQueryFriendsResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_SendFriendInvite* UAsyncAction_SendFriendInvite::SendFriendInvite(UObject* WorldContextObject,
                                                                             const FString& FriendUserId)
{
	UAsyncAction_SendFriendInvite* Action = NewObject<UAsyncAction_SendFriendInvite>();
	Action->WorldContext = WorldContextObject;
	Action->FriendUserId = FriendUserId;
	return Action;
}

void UAsyncAction_SendFriendInvite::Activate()
{
	IGamingService* Service = ResolveService();
	IFriendsService* Friends = Service ? Service->GetFriends() : nullptr;
	if (!Friends || FriendUserId.IsEmpty())
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Friends->SendInvite(FriendUserId, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

bool UFriendsLibrary::IsFriendsAvailable(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IFriendsService* Friends = Service ? Service->GetFriends() : nullptr;
	return Friends ? Friends->IsAvailable() : false;
}

TArray<FGamingFriend> UFriendsLibrary::GetCachedFriends(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IFriendsService* Friends = Service ? Service->GetFriends() : nullptr;
	return Friends ? Friends->GetCachedFriends() : TArray<FGamingFriend>();
}

int32 UFriendsLibrary::GetFriendCount(const TArray<FGamingFriend>& Friends)
{
	return Friends.Num();
}

FGamingFriend UFriendsLibrary::GetFriendAt(const TArray<FGamingFriend>& Friends, int32 Index)
{
	return Friends.IsValidIndex(Index) ? Friends[Index] : FGamingFriend();
}

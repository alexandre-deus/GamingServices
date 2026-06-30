#include "Blueprint/Libraries/UserLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IUserService.h"
#include "Blueprint/GamingPlatformSubsystem.h"

UAsyncAction_Login* UAsyncAction_Login::Login(UObject* WorldContextObject, const FGamingServiceLoginParams& Params)
{
	UAsyncAction_Login* Action = NewObject<UAsyncAction_Login>();
	Action->WorldContext = WorldContextObject;
	Action->Params = Params;
	return Action;
}

void UAsyncAction_Login::Activate()
{
	IGamingService* Service = ResolveService();
	IUserService* User = Service ? Service->GetUser() : nullptr;
	if (!User)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	User->Login(Params, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

bool UUserLibrary::IsLoggedIn(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IUserService* User = Service ? Service->GetUser() : nullptr;
	return User ? User->IsLoggedIn() : false;
}

bool UUserLibrary::NeedsLogin(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IUserService* User = Service ? Service->GetUser() : nullptr;
	return User ? User->NeedsLogin() : false;
}

FString UUserLibrary::GetUserId(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IUserService* User = Service ? Service->GetUser() : nullptr;
	return User ? User->GetUserId() : FString();
}

FString UUserLibrary::GetDisplayName(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IUserService* User = Service ? Service->GetUser() : nullptr;
	return User ? User->GetDisplayName() : FString();
}

UTexture2D* UUserLibrary::GetAvatar(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IUserService* User = Service ? Service->GetUser() : nullptr;
	return User ? User->GetAvatar() : nullptr;
}

UTexture2D* UUserLibrary::GetAvatarByUserId(const UObject* WorldContextObject, const FString& UserId)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IUserService* User = Service ? Service->GetUser() : nullptr;
	return User ? User->GetAvatarByUserId(UserId) : nullptr;
}

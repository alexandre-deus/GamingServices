#include "Blueprint/Libraries/EntitlementsLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IEntitlementsService.h"

UAsyncAction_ListEntitlements* UAsyncAction_ListEntitlements::ListEntitlements(UObject* WorldContextObject)
{
	UAsyncAction_ListEntitlements* Action = NewObject<UAsyncAction_ListEntitlements>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_ListEntitlements::Activate()
{
	IGamingService* Service = ResolveService();
	IEntitlementsService* Entitlements = Service ? Service->GetEntitlements() : nullptr;
	if (!Entitlements)
	{
		Completed.Broadcast(FEntitlementsListResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Entitlements->ListEntitlements([this](const FEntitlementsListResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_HasEntitlement* UAsyncAction_HasEntitlement::HasEntitlement(UObject* WorldContextObject, const FEntitlementDefinition& Definition)
{
	UAsyncAction_HasEntitlement* Action = NewObject<UAsyncAction_HasEntitlement>();
	Action->WorldContext = WorldContextObject;
	Action->Definition = Definition;
	return Action;
}

void UAsyncAction_HasEntitlement::Activate()
{
	IGamingService* Service = ResolveService();
	IEntitlementsService* Entitlements = Service ? Service->GetEntitlements() : nullptr;
	if (!Entitlements)
	{
		Completed.Broadcast(FHasEntitlementResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Entitlements->HasEntitlement(Definition, [this](const FHasEntitlementResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

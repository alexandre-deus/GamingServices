#pragma once

#include "CoreMinimal.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/EntitlementTypes.h"
#include "EntitlementsLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEntitlementsListedPin, const FEntitlementsListResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEntitlementCheckedPin, const FHasEntitlementResult&, Result);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_ListEntitlements : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FEntitlementsListedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Entitlements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ListEntitlements* ListEntitlements(UObject* WorldContextObject);

	virtual void Activate() override;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_HasEntitlement : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FEntitlementCheckedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Entitlements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_HasEntitlement* HasEntitlement(UObject* WorldContextObject, const FEntitlementDefinition& Definition);

	virtual void Activate() override;

private:
	UPROPERTY()
	FEntitlementDefinition Definition;
};

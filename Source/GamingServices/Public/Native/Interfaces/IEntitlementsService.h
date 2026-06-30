#pragma once

#include "CoreMinimal.h"
#include "DataTypes/EntitlementTypes.h"
#include "Native/GamingCapability.h"

/** DLC / entitlement listing + ownership-check capability. */
class GAMINGSERVICES_API IEntitlementsService
{
public:
	virtual ~IEntitlementsService() = default;

	virtual void ListEntitlements(TFunction<void(const FEntitlementsListResult&)> Callback) = 0;
	virtual void HasEntitlement(const FEntitlementDefinition& Definition,
	                            TFunction<void(const FHasEntitlementResult&)> Callback) = 0;
};

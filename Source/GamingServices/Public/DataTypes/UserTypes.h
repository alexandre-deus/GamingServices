#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "UserTypes.generated.h"

/**
 * Result of IUserService::ResolveDisplayName. DisplayName is always non-empty and displayable:
 * on failure it falls back to the UserId string, and bSuccess distinguishes a genuinely resolved
 * name (true) from that id fallback (false).
 */
USTRUCT(BlueprintType)
struct FResolveDisplayNameResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString UserId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	FResolveDisplayNameResult() = default;

	/** Success: a real name was resolved. */
	static FResolveDisplayNameResult Resolved(const FString& InUserId, const FString& InDisplayName)
	{
		FResolveDisplayNameResult R;
		R.bSuccess = true;
		R.UserId = InUserId;
		R.DisplayName = InDisplayName;
		return R;
	}

	/** Failure: no name available, DisplayName falls back to the id so callers still have a value. */
	static FResolveDisplayNameResult Fallback(const FString& InUserId)
	{
		FResolveDisplayNameResult R;
		R.bSuccess = false;
		R.UserId = InUserId;
		R.DisplayName = InUserId;
		return R;
	}
};

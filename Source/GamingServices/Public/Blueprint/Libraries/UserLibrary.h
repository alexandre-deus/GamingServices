#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/GamingServiceResult.h"
#include "DataTypes/LoginTypes.h"
#include "UserLibrary.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLoginResultPin, const FGamingServiceResult&, Result);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_Login : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FLoginResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_Login* Login(UObject* WorldContextObject, const FGamingServiceLoginParams& Params);

	virtual void Activate() override;

private:
	UPROPERTY()
	FGamingServiceLoginParams Params;
};

/** Synchronous user/identity getters (no async work). */
UCLASS()
class GAMINGSERVICES_API UUserLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "GamingServices|User", meta = (WorldContext = "WorldContextObject"))
	static bool IsLoggedIn(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "GamingServices|User", meta = (WorldContext = "WorldContextObject"))
	static bool NeedsLogin(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "GamingServices|User", meta = (WorldContext = "WorldContextObject"))
	static FString GetUserId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "GamingServices|User", meta = (WorldContext = "WorldContextObject"))
	static FString GetDisplayName(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "GamingServices|User", meta = (WorldContext = "WorldContextObject"))
	static UTexture2D* GetAvatar(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "GamingServices|User", meta = (WorldContext = "WorldContextObject"))
	static UTexture2D* GetAvatarByUserId(const UObject* WorldContextObject, const FString& UserId);
};

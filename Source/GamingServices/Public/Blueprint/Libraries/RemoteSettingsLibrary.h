#pragma once

#include "CoreMinimal.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/RemoteSettingTypes.h"
#include "RemoteSettingsLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRemoteSettingPin, const FRemoteSettingResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRemoteSettingsListedPin, const FRemoteSettingsListResult&, Result);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_SetRemoteSetting : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FRemoteSettingPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_SetRemoteSetting* SetRemoteSetting(UObject* WorldContextObject, const FString& Key, const FString& Value);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString Key;

	UPROPERTY()
	FString Value;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_GetRemoteSetting : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FRemoteSettingPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_GetRemoteSetting* GetRemoteSetting(UObject* WorldContextObject, const FString& Key);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString Key;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_DeleteRemoteSetting : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FRemoteSettingPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_DeleteRemoteSetting* DeleteRemoteSetting(UObject* WorldContextObject, const FString& Key);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString Key;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_ListRemoteSettings : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FRemoteSettingsListedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ListRemoteSettings* ListRemoteSettings(UObject* WorldContextObject);

	virtual void Activate() override;
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/CloudStorageTypes.h"
#include "CloudStorageLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCloudFileOpPin, const FGamingServiceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFileReadPin, const FFileReadResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFilesListedPin, const FFilesListResult&, Result);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_WriteFile : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FCloudFileOpPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_WriteFile* WriteFile(UObject* WorldContextObject, const FString& FilePath, const TArray<uint8>& Data);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString FilePath;

	TArray<uint8> Data;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_ReadFile : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FFileReadPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ReadFile* ReadFile(UObject* WorldContextObject, const FString& FilePath);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString FilePath;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_DeleteFile : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FCloudFileOpPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_DeleteFile* DeleteFile(UObject* WorldContextObject, const FString& FilePath);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString FilePath;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_ListFiles : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FFilesListedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Cloud", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ListFiles* ListFiles(UObject* WorldContextObject, const FString& DirectoryPath);

	virtual void Activate() override;

private:
	UPROPERTY()
	FString DirectoryPath;
};

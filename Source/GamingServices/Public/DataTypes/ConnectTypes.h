#pragma once

#include "CoreMinimal.h"
#include "ConnectTypes.generated.h"

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FEOSInitOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ProductName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ProductVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ProductId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SandboxId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DeploymentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ClientId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ClientSecret;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EncryptionKey;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSteamworksInitOptions
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FGamingServiceConnectParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEOSInitOptions EOS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSteamworksInitOptions Steamworks;
};

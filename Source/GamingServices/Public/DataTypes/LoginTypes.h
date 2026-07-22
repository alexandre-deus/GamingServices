#pragma once

#include "CoreMinimal.h"
#include "LoginTypes.generated.h"

UENUM(BlueprintType)
enum class EEOSLoginMethod : uint8
{
	PersistentAuth UMETA(DisplayName = "Persistent Auth"),
	AccountPortal UMETA(DisplayName = "Account Portal"),
	DeviceCode UMETA(DisplayName = "Device Code"),
	Developer UMETA(DisplayName = "Developer")
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FEOSLoginOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEOSLoginMethod Method = EEOSLoginMethod::PersistentAuth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DeveloperHost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DeveloperCredentialName;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSteamworksLoginOptions
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FGamingServiceLoginParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEOSLoginOptions EOS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSteamworksLoginOptions Steamworks;
};

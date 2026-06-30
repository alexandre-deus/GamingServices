#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "CloudStorageTypes.generated.h"

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FFileBlobData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString FilePath;

	UPROPERTY(BlueprintReadOnly)
	int64 Size;

	UPROPERTY(BlueprintReadOnly)
	FDateTime LastModified;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FFilesListResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FFileBlobData> Files;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FFileReadResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString FilePath;

	TArray<uint8> Data;

	FFileReadResult() = default;

	FFileReadResult(bool InSuccess, const FString& InFilePath = TEXT(""), const TArray<uint8>& InData = TArray<uint8>())
		: FGamingServiceResult(InSuccess), FilePath(InFilePath), Data(InData)
	{
	}
};

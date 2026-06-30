#pragma once

#include "CoreMinimal.h"
#include "DataTypes/CloudStorageTypes.h"
#include "Native/GamingCapability.h"

/** Remote/cloud file storage capability (write, read, delete, list). */
class GAMINGSERVICES_API ICloudStorageService
{
public:
	virtual ~ICloudStorageService() = default;

	virtual void WriteFile(const FString& FilePath, const TArray<uint8>& Data,
	                       TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void ReadFile(const FString& FilePath,
	                      TFunction<void(const FFileReadResult&)> Callback) = 0;
	virtual void DeleteFile(const FString& FilePath,
	                        TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void ListFiles(const FString& DirectoryPath,
	                       TFunction<void(const FFilesListResult&)> Callback) = 0;
};

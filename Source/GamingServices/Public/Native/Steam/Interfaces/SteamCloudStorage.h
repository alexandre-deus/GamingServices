#pragma once

#ifdef USE_STEAMWORKS

#include "CoreMinimal.h"
#include "Native/Interfaces/ICloudStorageService.h"

namespace GamingServices
{
	class FSteamPlatformCore;

	/** Steam Cloud file storage via the global SteamRemoteStorage() interface. */
	class FSteamCloudStorage final : public ICloudStorageService
	{
	public:
		explicit FSteamCloudStorage(FSteamPlatformCore& InCore) : Core(InCore) {}

		virtual void WriteFile(const FString& FilePath, const TArray<uint8>& Data,
		                       TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void ReadFile(const FString& FilePath,
		                      TFunction<void(const FFileReadResult&)> Callback) override;
		virtual void DeleteFile(const FString& FilePath,
		                        TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void ListFiles(const FString& DirectoryPath,
		                       TFunction<void(const FFilesListResult&)> Callback) override;

	private:
		FSteamPlatformCore& Core;
	};
}

#endif // USE_STEAMWORKS

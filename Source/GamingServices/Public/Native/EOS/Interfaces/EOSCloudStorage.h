#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"
#include "Native/Interfaces/ICloudStorageService.h"

namespace GamingServices
{
	class FEOSPlatformCore;

	/**
	 * EOS cloud-storage capability.
	 *
	 * Files live locally under the core's temp-storage directory; the EOS PlayerDataStorage interface is
	 * used to sync that directory to/from the cloud, tracked by a manifest.json that records each file's
	 * timestamp + size. Sync is driven by the platform core (on login / shutdown) through the core's
	 * SyncFromCloudHook / SyncToCloudHook, which this class binds in its constructor.
	 */
	class FEOSCloudStorage final : public ICloudStorageService
	{
	public:
		explicit FEOSCloudStorage(FEOSPlatformCore& InCore);

		virtual void WriteFile(const FString& FilePath, const TArray<uint8>& Data,
		                       TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void ReadFile(const FString& FilePath,
		                      TFunction<void(const FFileReadResult&)> Callback) override;
		virtual void DeleteFile(const FString& FilePath,
		                        TFunction<void(const FGamingServiceResult&)> Callback) override;
		virtual void ListFiles(const FString& DirectoryPath,
		                       TFunction<void(const FFilesListResult&)> Callback) override;

		// Sync orchestration (bound to the core's hooks).
		void SyncFromCloud(TFunction<void(const FGamingServiceResult&)> Callback);
		void SyncToCloud(TFunction<void(const FGamingServiceResult&)> Callback);

	private:
		struct FFileManifestEntry
		{
			int64 Timestamp;
			int64 Size;
		};

		struct FCloudManifest
		{
			TMap<FString, FFileManifestEntry> Files;
			int64 LastSyncTime = 0;

			FString ToJson() const;
			bool FromJson(const FString& JsonString);
		};

		FCloudManifest BuildLocalManifest();
		bool SaveLocalManifest(const FCloudManifest& Manifest);
		bool LoadLocalManifest(FCloudManifest& OutManifest);

		void DownloadFromCloudGeneric(const FString& FileName, TFunction<void(bool, const TArray<uint8>&)> Callback);
		void UploadToCloudGeneric(const FString& FileName, const TArray<uint8>& FileData, TFunction<void(bool)> Callback);
		void DownloadManifestFromCloud(TFunction<void(bool, const FCloudManifest&)> Callback);
		void UploadManifestToCloud(const FCloudManifest& Manifest, TFunction<void(bool)> Callback);
		void DownloadFileFromCloud(const FString& FileName, TFunction<void(bool)> Callback);
		void UploadFileToCloud(const FString& FileName, TFunction<void(bool)> Callback);
		void DeleteFileFromCloud(const FString& FileName, TFunction<void(bool)> Callback);

		void ProcessSyncOperations(const TArray<FString>& FilesToDownload, const TArray<FString>& FilesToDelete,
		                           int32 DownloadIndex, int32 DeleteIndex,
		                           TFunction<void(const FGamingServiceResult&)> Callback);
		void ExecuteShutdownSync(const TArray<FString>& FilesToUpload, const TArray<FString>& FilesToDelete,
		                         int32 UploadIndex, int32 DeleteIndex,
		                         TFunction<void(const FGamingServiceResult&)> Callback);

		FEOSPlatformCore& Core;
	};
}

#endif // GS_WITH_EOS

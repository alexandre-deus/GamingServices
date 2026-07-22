#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSCloudStorage.h"
#include "EOSCommon.h"
#include "EOSCallbackContext.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

#include <string>

namespace GamingServices
{
	using FFileStorageCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSCloudStorage>;

	// Cast the core's opaque accessors back to their EOS_* types in this .cpp so the core header stays SDK-free.
	static EOS_HPlayerDataStorage PlayerDataStorageHandle(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_HPlayerDataStorage>(Core.GetPlayerDataStorageHandle());
	}

	FEOSCloudStorage::FEOSCloudStorage(FEOSPlatformCore& InCore)
		: Core(InCore)
	{
		// Drive cloud sync from the core's login / shutdown flow.
		Core.SyncFromCloudHook = [this](TFunction<void(const FGamingServiceResult&)> Callback)
		{
			SyncFromCloud(MoveTemp(Callback));
		};
		Core.SyncToCloudHook = [this](TFunction<void(const FGamingServiceResult&)> Callback)
		{
			SyncToCloud(MoveTemp(Callback));
		};
	}

	void FEOSCloudStorage::WriteFile(const FString& FilePath, const TArray<uint8>& Data,
	                                 TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized(), TEXT("EOSGamingService: WriteFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Writing file to cloud storage: %s (%d bytes)"), *FilePath, Data.Num());

		FString FullPath = Core.GetFullLocalPath(FilePath);

		if (!FFileHelper::SaveArrayToFile(Data, *FullPath))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to write to cloud storage: %s"), *FullPath);
			Callback(FGamingServiceResult(false));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File written to cloud storage successfully"));
		Callback(FGamingServiceResult(true));
	}

	void FEOSCloudStorage::ReadFile(const FString& FilePath,
	                                TFunction<void(const FFileReadResult&)> Callback)
	{
		checkf(Core.IsInitialized(), TEXT("EOSGamingService: ReadFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Reading file from cloud storage: %s"), *FilePath);

		FString FullPath = Core.GetFullLocalPath(FilePath);

		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FullPath))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to read from cloud storage: %s"), *FullPath);
			Callback(FFileReadResult(false, FilePath));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File read from cloud storage successfully (%d bytes)"), FileData.Num());
		Callback(FFileReadResult(true, FilePath, FileData));
	}

	void FEOSCloudStorage::DeleteFile(const FString& FilePath,
	                                  TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized(), TEXT("EOSGamingService: DeleteFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Deleting file from local storage: %s"), *FilePath);

		FString FullPath = Core.GetFullLocalPath(FilePath);

		if (IFileManager::Get().Delete(*FullPath))
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File deleted from local storage successfully"));
			Callback(FGamingServiceResult(true));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to delete file from local storage: %s"), *FullPath);
			Callback(FGamingServiceResult(false));
		}
	}

	void FEOSCloudStorage::ListFiles(const FString& DirectoryPath,
	                                 TFunction<void(const FFilesListResult&)> Callback)
	{
		checkf(Core.IsInitialized(), TEXT("EOSGamingService: ListFiles called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Listing files in local storage directory: %s"), *DirectoryPath);

		const FString& TempStoragePath = Core.GetTempStoragePath();
		FString FullDirectoryPath = TempStoragePath.IsEmpty()
			? (FPaths::ProjectSavedDir() / FEOSPlatformCore::CloudStorageDirectoryName / DirectoryPath)
			: (TempStoragePath / DirectoryPath);

		FFilesListResult Result;
		Result.bSuccess = true;

		if (!FPaths::DirectoryExists(FullDirectoryPath))
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Directory does not exist: %s"), *FullDirectoryPath);
			Callback(Result);
			return;
		}

		TArray<FString> FoundFiles;
		IFileManager::Get().FindFiles(FoundFiles, *(FullDirectoryPath / TEXT("*")), true, false);

		for (const FString& File : FoundFiles)
		{
			FString FilePath = DirectoryPath.IsEmpty() ? File : (DirectoryPath / File);
			FString FullFilePath = FullDirectoryPath / File;

			FFileBlobData FileData;
			FileData.FilePath = FilePath;

			const int64 FileSize = IFileManager::Get().FileSize(*FullFilePath);
			FileData.Size = FileSize > 0 ? FileSize : 0;

			FileData.LastModified = IFileManager::Get().GetTimeStamp(*FullFilePath);

			Result.Files.Add(FileData);
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Found %d files in local storage"), Result.Files.Num());
		Callback(Result);
	}

	// ------------------------------------------------------------------------
	// Manifest
	// ------------------------------------------------------------------------

	FString FEOSCloudStorage::FCloudManifest::ToJson() const
	{
		TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());

		TSharedPtr<FJsonObject> FilesObject = MakeShareable(new FJsonObject());
		for (const auto& Pair : Files)
		{
			TSharedPtr<FJsonObject> FileEntry = MakeShareable(new FJsonObject());
			FileEntry->SetNumberField(TEXT("timestamp"), Pair.Value.Timestamp);
			FileEntry->SetNumberField(TEXT("size"), Pair.Value.Size);
			FilesObject->SetObjectField(Pair.Key, FileEntry);
		}

		RootObject->SetObjectField(TEXT("files"), FilesObject);
		RootObject->SetNumberField(TEXT("last_sync_time"), LastSyncTime);

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		if (FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
		{
			return OutputString;
		}
		return TEXT("{}");
	}

	bool FEOSCloudStorage::FCloudManifest::FromJson(const FString& JsonString)
	{
		Files.Empty();
		LastSyncTime = 0;

		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			return false;
		}

		if (RootObject->HasTypedField<EJson::Number>(TEXT("last_sync_time")))
		{
			LastSyncTime = static_cast<int64>(RootObject->GetNumberField(TEXT("last_sync_time")));
		}

		if (RootObject->HasTypedField<EJson::Object>(TEXT("files")))
		{
			TSharedPtr<FJsonObject> FilesObject = RootObject->GetObjectField(TEXT("files"));
			for (const auto& FilePair : FilesObject->Values)
			{
				if (FilePair.Value->Type == EJson::Object)
				{
					TSharedPtr<FJsonObject> FileEntry = FilePair.Value->AsObject();

					FFileManifestEntry Entry;
					Entry.Timestamp = static_cast<int64>(FileEntry->GetNumberField(TEXT("timestamp")));
					Entry.Size = static_cast<int64>(FileEntry->GetNumberField(TEXT("size")));

					Files.Add(FilePair.Key, Entry);
				}
			}
		}

		return true;
	}

	FEOSCloudStorage::FCloudManifest FEOSCloudStorage::BuildLocalManifest()
	{
		FCloudManifest Manifest;
		const FString& TempStoragePath = Core.GetTempStoragePath();
		FString BasePath = TempStoragePath.IsEmpty()
			                   ? (FPaths::ProjectSavedDir() / FEOSPlatformCore::CloudStorageDirectoryName)
			                   : TempStoragePath;

		UE_LOG(LogTemp, VeryVerbose, TEXT("EOSGamingService: Building local manifest from: %s"), *BasePath);

		if (!FPaths::DirectoryExists(BasePath))
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Base directory does not exist: %s"), *BasePath);
			return Manifest;
		}

		TArray<FString> LocalFiles;
		IFileManager::Get().FindFilesRecursive(LocalFiles, *BasePath, TEXT("*"), true, false);
		UE_LOG(LogTemp, VeryVerbose, TEXT("EOSGamingService: Found %d files in directory"), LocalFiles.Num());

		FString BasePathWithSlash = BasePath;
		if (!BasePathWithSlash.EndsWith(TEXT("/")) && !BasePathWithSlash.EndsWith(TEXT("\\")))
		{
			BasePathWithSlash += TEXT("/");
		}

		for (const FString& FullPath : LocalFiles)
		{
			FString RelativePath = FullPath;
			if (RelativePath.StartsWith(BasePathWithSlash))
			{
				RelativePath = RelativePath.RightChop(BasePathWithSlash.Len());
			}
			else if (RelativePath.StartsWith(BasePath))
			{
				RelativePath = RelativePath.RightChop(BasePath.Len());
				if (RelativePath.StartsWith(TEXT("/")) || RelativePath.StartsWith(TEXT("\\")))
				{
					RelativePath = RelativePath.RightChop(1);
				}
			}

			if (RelativePath == FEOSPlatformCore::ManifestFileName)
				continue;

			FFileManifestEntry Entry;
			Entry.Timestamp = IFileManager::Get().GetTimeStamp(*FullPath).ToUnixTimestamp();
			Entry.Size = IFileManager::Get().FileSize(*FullPath);

			if (Entry.Size > 0)
			{
				Manifest.Files.Add(RelativePath, Entry);
			}
		}

		Manifest.LastSyncTime = FDateTime::UtcNow().ToUnixTimestamp();
		return Manifest;
	}

	bool FEOSCloudStorage::SaveLocalManifest(const FCloudManifest& Manifest)
	{
		const FString& TempStoragePath = Core.GetTempStoragePath();
		FString BasePath = TempStoragePath.IsEmpty()
			                   ? (FPaths::ProjectSavedDir() / FEOSPlatformCore::CloudStorageDirectoryName)
			                   : TempStoragePath;
		FString ManifestPath = BasePath / FEOSPlatformCore::ManifestFileName;

		IFileManager::Get().MakeDirectory(*BasePath, true);
		return FFileHelper::SaveStringToFile(Manifest.ToJson(), *ManifestPath);
	}

	bool FEOSCloudStorage::LoadLocalManifest(FCloudManifest& OutManifest)
	{
		const FString& TempStoragePath = Core.GetTempStoragePath();
		FString BasePath = TempStoragePath.IsEmpty()
			                   ? (FPaths::ProjectSavedDir() / FEOSPlatformCore::CloudStorageDirectoryName)
			                   : TempStoragePath;
		FString ManifestPath = BasePath / FEOSPlatformCore::ManifestFileName;

		FString JsonContent;
		if (!FFileHelper::LoadFileToString(JsonContent, *ManifestPath))
		{
			return false;
		}

		return OutManifest.FromJson(JsonContent);
	}

	// ------------------------------------------------------------------------
	// PlayerDataStorage transfer
	// ------------------------------------------------------------------------

	void FEOSCloudStorage::DownloadFromCloudGeneric(const FString& FileName,
	                                                TFunction<void(bool, const TArray<uint8>&)> Callback)
	{
		if (!Core.IsLoggedIn() || !Core.GetProductUserId() || !Core.GetPlayerDataStorageHandle())
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot download file %s - not logged in"), *FileName);
			Callback(false, TArray<uint8>());
			return;
		}

		struct FGenericDownloadCtx : FFileStorageCallbackCtx
		{
			FString FileName;
			TArray<uint8> FileData;
			TFunction<void(bool, const TArray<uint8>&)> DownloadCallback;
		};
		auto* Ctx = new FGenericDownloadCtx{};
		Ctx->Service = this;
		Ctx->FileName = FileName;
		Ctx->DownloadCallback = MoveTemp(Callback);

		// Must outlive the EOS_PlayerDataStorage_ReadFile call below; assigning TCHAR_TO_UTF8()
		// directly leaves a dangling pointer.
		const std::string FileNameUtf8 = TCHAR_TO_UTF8(*FileName);

		EOS_PlayerDataStorage_ReadFileOptions ReadOptions = {};
		ReadOptions.ApiVersion = 1;
		ReadOptions.LocalUserId = ProductUserId(Core);
		ReadOptions.Filename = FileNameUtf8.c_str();
		ReadOptions.ReadChunkLengthBytes = 1024 * 1024;
		ReadOptions.ReadFileDataCallback = [](
			const EOS_PlayerDataStorage_ReadFileDataCallbackInfo* Data) -> EOS_PlayerDataStorage_EReadResult
			{
				if (Data && Data->ClientData)
				{
					auto* LocalCtx = static_cast<FGenericDownloadCtx*>(Data->ClientData);
					if (Data->DataChunk && Data->DataChunkLengthBytes > 0)
					{
						LocalCtx->FileData.Append(static_cast<const uint8*>(Data->DataChunk),
						                          Data->DataChunkLengthBytes);
					}
				}
				return EOS_PlayerDataStorage_EReadResult::EOS_RR_ContinueReading;
			};
		ReadOptions.FileTransferProgressCallback = nullptr;

		EOS_PlayerDataStorage_ReadFile(
			PlayerDataStorageHandle(Core),
			&ReadOptions,
			Ctx,
			[](const EOS_PlayerDataStorage_ReadFileCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FGenericDownloadCtx*>(Data->ClientData);

				bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!bSuccess && Data->ResultCode != EOS_EResult::EOS_NotFound)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to download %s: %d"), *LocalCtx->FileName,
					       (int32)Data->ResultCode);
				}

				LocalCtx->DownloadCallback(bSuccess, LocalCtx->FileData);
				delete LocalCtx;
			}
		);
	}

	void FEOSCloudStorage::UploadToCloudGeneric(const FString& FileName, const TArray<uint8>& FileData,
	                                            TFunction<void(bool)> Callback)
	{
		if (!Core.IsLoggedIn() || !Core.GetProductUserId() || !Core.GetPlayerDataStorageHandle())
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot upload file %s - not logged in"), *FileName);
			Callback(false);
			return;
		}

		struct FGenericUploadCtx : FFileStorageCallbackCtx
		{
			TArray<uint8> FileData;
			uint32_t CurrentOffset = 0;
			FString FileName;
			TFunction<void(bool)> UploadCallback;
		};
		auto* Ctx = new FGenericUploadCtx{};
		Ctx->Service = this;
		Ctx->FileData = FileData;
		Ctx->FileName = FileName;
		Ctx->UploadCallback = MoveTemp(Callback);

		// Must outlive the EOS_PlayerDataStorage_WriteFile call below; assigning TCHAR_TO_UTF8()
		// directly leaves a dangling pointer.
		const std::string FileNameUtf8 = TCHAR_TO_UTF8(*FileName);

		EOS_PlayerDataStorage_WriteFileOptions WriteOptions = {};
		WriteOptions.ApiVersion = 1;
		WriteOptions.LocalUserId = ProductUserId(Core);
		WriteOptions.Filename = FileNameUtf8.c_str();
		WriteOptions.ChunkLengthBytes = 4096;
		WriteOptions.WriteFileDataCallback = [](const EOS_PlayerDataStorage_WriteFileDataCallbackInfo* Data,
		                                        void* OutDataBuffer,
		                                        uint32_t* OutDataWritten) -> EOS_PlayerDataStorage_EWriteResult
		{
			if (Data && Data->ClientData && OutDataBuffer && OutDataWritten)
			{
				auto* LocalCtx = static_cast<FGenericUploadCtx*>(Data->ClientData);
				const uint32_t RemainingBytes = LocalCtx->FileData.Num() - LocalCtx->CurrentOffset;
				const uint32_t BytesToWrite = FMath::Min(Data->DataBufferLengthBytes, RemainingBytes);

				if (BytesToWrite > 0)
				{
					FMemory::Memcpy(OutDataBuffer, LocalCtx->FileData.GetData() + LocalCtx->CurrentOffset,
					                BytesToWrite);
					LocalCtx->CurrentOffset += BytesToWrite;
					*OutDataWritten = BytesToWrite;
					return EOS_PlayerDataStorage_EWriteResult::EOS_WR_ContinueWriting;
				}
			}
			*OutDataWritten = 0;
			return EOS_PlayerDataStorage_EWriteResult::EOS_WR_CompleteRequest;
		};
		WriteOptions.FileTransferProgressCallback = nullptr;

		EOS_PlayerDataStorage_WriteFile(
			PlayerDataStorageHandle(Core),
			&WriteOptions,
			Ctx,
			[](const EOS_PlayerDataStorage_WriteFileCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FGenericUploadCtx*>(Data->ClientData);

				bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to upload %s: %d"), *LocalCtx->FileName,
					       (int32)Data->ResultCode);
				}

				LocalCtx->UploadCallback(bSuccess);
				delete LocalCtx;
			}
		);
	}

	void FEOSCloudStorage::DownloadManifestFromCloud(TFunction<void(bool, const FCloudManifest&)> Callback)
	{
		DownloadFromCloudGeneric(FEOSPlatformCore::ManifestFileName,
			[Callback](bool bSuccess, const TArray<uint8>& FileData)
		{
			FCloudManifest Manifest;

			if (bSuccess && FileData.Num() > 0)
			{
				FString JsonContent;
				FFileHelper::BufferToString(JsonContent, FileData.GetData(), FileData.Num());
				bSuccess = Manifest.FromJson(JsonContent);

				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Downloaded manifest from cloud with %d files"),
					       Manifest.Files.Num());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to parse manifest JSON"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: No manifest found in cloud (first sync)"));
				bSuccess = true;
				Manifest = FCloudManifest();
			}

			Callback(bSuccess, Manifest);
		});
	}

	void FEOSCloudStorage::UploadManifestToCloud(const FCloudManifest& Manifest, TFunction<void(bool)> Callback)
	{
		FString JsonContent = Manifest.ToJson();
		TArray<uint8> FileData;
		FileData.Append((uint8*)TCHAR_TO_UTF8(*JsonContent), JsonContent.Len());

		UploadToCloudGeneric(FEOSPlatformCore::ManifestFileName, FileData, [Callback](bool bSuccess)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Uploaded manifest to cloud"));
			}
			Callback(bSuccess);
		});
	}

	void FEOSCloudStorage::DownloadFileFromCloud(const FString& FileName, TFunction<void(bool)> Callback)
	{
		DownloadFromCloudGeneric(FileName, [this, FileName, Callback](bool bSuccess, const TArray<uint8>& FileData)
		{
			if (bSuccess && FileData.Num() > 0)
			{
				FString FullPath = Core.GetFullLocalPath(FileName);
				FString Directory = FPaths::GetPath(FullPath);
				IFileManager::Get().MakeDirectory(*Directory, true);

				if (FFileHelper::SaveArrayToFile(FileData, *FullPath))
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Downloaded file: %s"), *FileName);
					Callback(true);
					return;
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to save downloaded file: %s"), *FileName);
				}
			}

			Callback(false);
		});
	}

	void FEOSCloudStorage::UploadFileToCloud(const FString& FileName, TFunction<void(bool)> Callback)
	{
		FString FullPath = Core.GetFullLocalPath(FileName);

		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FullPath))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to read file for upload: %s"), *FileName);
			Callback(false);
			return;
		}

		UploadToCloudGeneric(FileName, FileData, [FileName, Callback](bool bSuccess)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Uploaded file: %s"), *FileName);
			}
			Callback(bSuccess);
		});
	}

	void FEOSCloudStorage::DeleteFileFromCloud(const FString& FileName, TFunction<void(bool)> Callback)
	{
		if (!Core.IsLoggedIn() || !Core.GetProductUserId() || !Core.GetPlayerDataStorageHandle())
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot delete file %s - not logged in"), *FileName);
			Callback(false);
			return;
		}

		struct FDeleteCtx : FFileStorageCallbackCtx
		{
			FString FileName;
			TFunction<void(bool)> DeleteCallback;
		};
		auto* Ctx = new FDeleteCtx{};
		Ctx->Service = this;
		Ctx->FileName = FileName;
		Ctx->DeleteCallback = MoveTemp(Callback);

		// Must outlive the EOS_PlayerDataStorage_DeleteFile call below; assigning TCHAR_TO_UTF8()
		// directly leaves a dangling pointer.
		const std::string FileNameUtf8 = TCHAR_TO_UTF8(*FileName);

		EOS_PlayerDataStorage_DeleteFileOptions DeleteOptions = {};
		DeleteOptions.ApiVersion = 1;
		DeleteOptions.LocalUserId = ProductUserId(Core);
		DeleteOptions.Filename = FileNameUtf8.c_str();

		EOS_PlayerDataStorage_DeleteFile(
			PlayerDataStorageHandle(Core),
			&DeleteOptions,
			Ctx,
			[](const EOS_PlayerDataStorage_DeleteFileCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FDeleteCtx*>(Data->ClientData);

				bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Deleted file from cloud: %s"), *LocalCtx->FileName);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to delete file %s: %d"),
					       *LocalCtx->FileName, (int32)Data->ResultCode);
				}

				LocalCtx->DeleteCallback(bSuccess);
				delete LocalCtx;
			}
		);
	}

	// ------------------------------------------------------------------------
	// Sync orchestration
	// ------------------------------------------------------------------------

	void FEOSCloudStorage::SyncFromCloud(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetPlayerDataStorageHandle(),
		       TEXT("EOSGamingService: SyncFromCloud called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting sync from cloud..."));

		DownloadManifestFromCloud([this, Callback](bool bSuccess, const FCloudManifest& CloudManifest)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to download cloud manifest"));
				Callback(FGamingServiceResult(false));
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cloud manifest contains %d files"), CloudManifest.Files.Num());

			FCloudManifest LocalManifest = BuildLocalManifest();
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Local manifest contains %d files"), LocalManifest.Files.Num());

			TArray<FString> FilesToDownload;
			TArray<FString> FilesToDelete;
			for (const auto& CloudFilePair : CloudManifest.Files)
			{
				const FString& FileName = CloudFilePair.Key;
				const FFileManifestEntry& CloudEntry = CloudFilePair.Value;

				if (FileName == FEOSPlatformCore::ManifestFileName)
				{
					continue;
				}

				if (!LocalManifest.Files.Contains(FileName))
				{
					FilesToDownload.Add(FileName);
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: File missing locally, will download: %s"), *FileName);
				}
				else
				{
					const FFileManifestEntry& LocalEntry = LocalManifest.Files[FileName];

					if (CloudEntry.Timestamp > LocalEntry.Timestamp)
					{
						FilesToDownload.Add(FileName);
						UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Cloud file is newer, will download: %s (cloud: %lld, local: %lld)"),
						       *FileName, CloudEntry.Timestamp, LocalEntry.Timestamp);
					}
				}
			}

			for (const auto& LocalFilePair : LocalManifest.Files)
			{
				const FString& FileName = LocalFilePair.Key;

				if (FileName == FEOSPlatformCore::ManifestFileName)
				{
					continue;
				}

				if (!CloudManifest.Files.Contains(FileName))
				{
					FilesToDelete.Add(FileName);
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Local file not in cloud, will delete: %s"), *FileName);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Sync plan - %d files to download, %d files to delete"),
			       FilesToDownload.Num(), FilesToDelete.Num());

			ProcessSyncOperations(FilesToDownload, FilesToDelete, 0, 0, Callback);
		});
	}

	void FEOSCloudStorage::ProcessSyncOperations(const TArray<FString>& FilesToDownload, const TArray<FString>& FilesToDelete,
	                                            int32 DownloadIndex, int32 DeleteIndex,
	                                            TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (DownloadIndex < FilesToDownload.Num())
		{
			const FString& FileName = FilesToDownload[DownloadIndex];
			DownloadFileFromCloud(FileName, [this, FilesToDownload, FilesToDelete, DownloadIndex, DeleteIndex, FileName, Callback](bool bSuccess)
			{
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to download file during sync: %s"), *FileName);
					Callback(FGamingServiceResult(false));
					return;
				}
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Downloaded file: %s"), *FileName);
				ProcessSyncOperations(FilesToDownload, FilesToDelete, DownloadIndex + 1, DeleteIndex, Callback);
			});
			return;
		}

		if (DeleteIndex < FilesToDelete.Num())
		{
			const FString& FileName = FilesToDelete[DeleteIndex];
			DeleteFile(FileName, [this, FilesToDownload, FilesToDelete, DownloadIndex, DeleteIndex, FileName, Callback](const FGamingServiceResult& Result)
			{
				if (!Result.bSuccess)
				{
					UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Failed to delete file during sync: %s"), *FileName);
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Deleted file: %s"), *FileName);
				}
				ProcessSyncOperations(FilesToDownload, FilesToDelete, DownloadIndex, DeleteIndex + 1, Callback);
			});
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Regenerating local manifest..."));
		FCloudManifest UpdatedManifest = BuildLocalManifest();

		if (SaveLocalManifest(UpdatedManifest))
		{
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Local manifest regenerated with %d files"), UpdatedManifest.Files.Num());
			Callback(FGamingServiceResult(true));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to save regenerated local manifest"));
			Callback(FGamingServiceResult(false));
		}
	}

	void FEOSCloudStorage::SyncToCloud(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (!Core.IsInitialized() || !Core.IsLoggedIn() || !Core.GetProductUserId() || !Core.GetPlayerDataStorageHandle())
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Cannot sync to cloud - not logged in or service not ready"));
			Callback(FGamingServiceResult(false));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Starting shutdown sync to cloud..."));

		FCloudManifest OldLocalManifest;
		LoadLocalManifest(OldLocalManifest);

		FCloudManifest CurrentLocalManifest = BuildLocalManifest();

		TArray<FString> FilesToUpload;
		for (const auto& LocalFile : CurrentLocalManifest.Files)
		{
			const FString& FileName = LocalFile.Key;
			const FFileManifestEntry& CurrentEntry = LocalFile.Value;

			if (!OldLocalManifest.Files.Contains(FileName))
			{
				FilesToUpload.Add(FileName);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Will upload new file: %s"), *FileName);
			}
			else
			{
				const FFileManifestEntry& OldEntry = OldLocalManifest.Files[FileName];
				if (OldEntry.Timestamp != CurrentEntry.Timestamp || OldEntry.Size != CurrentEntry.Size)
				{
					FilesToUpload.Add(FileName);
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Will upload modified file: %s"), *FileName);
				}
			}
		}

		TArray<FString> FilesToDelete;
		for (const auto& OldFile : OldLocalManifest.Files)
		{
			if (!CurrentLocalManifest.Files.Contains(OldFile.Key))
			{
				FilesToDelete.Add(OldFile.Key);
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Will delete removed file: %s"), *OldFile.Key);
			}
		}

		ExecuteShutdownSync(FilesToUpload, FilesToDelete, 0, 0, Callback);
	}

	void FEOSCloudStorage::ExecuteShutdownSync(const TArray<FString>& FilesToUpload, const TArray<FString>& FilesToDelete,
	                                          int32 UploadIndex, int32 DeleteIndex,
	                                          TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (UploadIndex < FilesToUpload.Num())
		{
			const FString& FileName = FilesToUpload[UploadIndex];
			UploadFileToCloud(
				FileName,
				[this, FilesToUpload, FilesToDelete, UploadIndex, DeleteIndex, Callback](bool bSuccess)
				{
					if (!bSuccess)
					{
						UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Shutdown sync failed during upload"));
						Callback(FGamingServiceResult(false));
						return;
					}
					ExecuteShutdownSync(FilesToUpload, FilesToDelete, UploadIndex + 1, DeleteIndex,
					                    Callback);
				});
			return;
		}

		if (DeleteIndex < FilesToDelete.Num())
		{
			const FString& FileName = FilesToDelete[DeleteIndex];
			DeleteFileFromCloud(
				FileName,
				[this, FilesToUpload, FilesToDelete, UploadIndex, DeleteIndex, Callback](bool bSuccess)
				{
					if (!bSuccess)
					{
						UE_LOG(LogTemp, Warning,
						       TEXT("EOSGamingService: Failed to delete file during shutdown sync, continuing"));
					}
					ExecuteShutdownSync(FilesToUpload, FilesToDelete, UploadIndex, DeleteIndex + 1,
					                    Callback);
				});
			return;
		}

		FCloudManifest FinalManifest = BuildLocalManifest();
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Shutdown sync - saving manifest with %d files"), FinalManifest.Files.Num());
		SaveLocalManifest(FinalManifest);
		UploadManifestToCloud(FinalManifest, [Callback](bool bSuccess)
		{
			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Shutdown sync completed successfully"));
				Callback(FGamingServiceResult(true));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to upload final manifest"));
				Callback(FGamingServiceResult(false));
			}
		});
	}
}

#endif // USE_EOS

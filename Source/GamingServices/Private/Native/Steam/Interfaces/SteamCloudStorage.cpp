#ifdef USE_STEAMWORKS

#include "Native/Steam/Interfaces/SteamCloudStorage.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	void FSteamCloudStorage::WriteFile(const FString& FilePath, const TArray<uint8>& Data,
									   TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamRemoteStorage* SteamRemoteStorage = ::SteamRemoteStorage();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: WriteFile called when service not "
					"ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Writing file to cloud: %s (%d bytes)"), *FilePath,
			   Data.Num());

		FTCHARToUTF8 UTF8String(*FilePath);
		const char* FilePathUTF8 = UTF8String.Get();

		bool bSuccess = SteamRemoteStorage->FileWrite(FilePathUTF8, Data.GetData(), Data.Num());

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log,
				   TEXT("SteamworksGamingService: File written to cloud "
						"successfully: %s"),
				   *FilePath);
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
				   TEXT("SteamworksGamingService: Failed to write file to cloud "
						"storage: %s"),
				   *FilePath);
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
		}
	}

	void FSteamCloudStorage::ReadFile(const FString& FilePath, TFunction<void(const FFileReadResult&)> Callback)
	{
		ISteamRemoteStorage* SteamRemoteStorage = ::SteamRemoteStorage();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: ReadFile called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Reading file from cloud: %s"), *FilePath);

		FTCHARToUTF8 UTF8String(*FilePath);
		const char* FilePathUTF8 = UTF8String.Get();

		bool bFileExists = SteamRemoteStorage->FileExists(FilePathUTF8);
		if (!bFileExists)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("SteamworksGamingService: File does not exist in cloud "
						"storage: %s"),
				   *FilePath);
			if (Callback)
			{
				Callback(FFileReadResult(false, FilePath));
			}
			return;
		}

		int32 FileSize = SteamRemoteStorage->GetFileSize(FilePathUTF8);
		if (FileSize <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Invalid file size for: %s"), *FilePath);
			if (Callback)
			{
				Callback(FFileReadResult(false, FilePath));
			}
			return;
		}

		TArray<uint8> FileData;
		FileData.SetNum(FileSize);
		int32 BytesRead = SteamRemoteStorage->FileRead(FilePathUTF8, FileData.GetData(), FileSize);

		if (BytesRead != FileSize)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to read file from cloud: %s"), *FilePath);
			if (Callback)
			{
				Callback(FFileReadResult(false, FilePath));
			}
			return;
		}

		UE_LOG(LogTemp, Log,
			   TEXT("SteamworksGamingService: File read from cloud successfully: "
					"%s (%d bytes)"),
			   *FilePath, FileSize);
		if (Callback)
		{
			Callback(FFileReadResult(true, FilePath, FileData));
		}
	}

	void FSteamCloudStorage::DeleteFile(const FString& FilePath, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamRemoteStorage* SteamRemoteStorage = ::SteamRemoteStorage();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: DeleteFile called when service not "
					"ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Deleting file: %s"), *FilePath);

		FTCHARToUTF8 UTF8String(*FilePath);
		const char* FilePathUTF8 = UTF8String.Get();

		bool bSuccess = SteamRemoteStorage->FileDelete(FilePathUTF8);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: File deleted successfully: %s"), *FilePath);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("SteamworksGamingService: File deletion failed or file did "
						"not exist: %s"),
				   *FilePath);
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void FSteamCloudStorage::ListFiles(const FString& DirectoryPath, TFunction<void(const FFilesListResult&)> Callback)
	{
		ISteamRemoteStorage* SteamRemoteStorage = ::SteamRemoteStorage();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamRemoteStorage,
			   TEXT("SteamworksGamingService: ListFiles called when service not "
					"ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Listing files in directory: %s"), *DirectoryPath);

		FFilesListResult Result;
		Result.bSuccess = true;

		int32 FileCount = SteamRemoteStorage->GetFileCount();

		for (int32 i = 0; i < FileCount; ++i)
		{
			int32 FileSize = 0;
			const char* FileName = SteamRemoteStorage->GetFileNameAndSize(i, &FileSize);

			if (FileName && FileSize > 0)
			{
				FString FileNameStr = UTF8_TO_TCHAR(FileName);

				if (DirectoryPath.IsEmpty() || FileNameStr.StartsWith(DirectoryPath))
				{
					FFileBlobData FileData;
					FileData.FilePath = FileNameStr;
					FileData.Size = FileSize;

					int64 Timestamp = SteamRemoteStorage->GetFileTimestamp(FileName);
					if (Timestamp > 0)
					{
						FileData.LastModified = FDateTime::FromUnixTimestamp(Timestamp);
					}
					else
					{
						FileData.LastModified = FDateTime::Now();
					}

					Result.Files.Add(FileData);
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Found %d files"), Result.Files.Num());
		if (Callback)
		{
			Callback(Result);
		}
	}
}

#endif // USE_STEAMWORKS

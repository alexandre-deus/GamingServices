#include "Blueprint/Libraries/CloudStorageLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/ICloudStorageService.h"

UAsyncAction_WriteFile* UAsyncAction_WriteFile::WriteFile(UObject* WorldContextObject, const FString& FilePath, const TArray<uint8>& Data)
{
	UAsyncAction_WriteFile* Action = NewObject<UAsyncAction_WriteFile>();
	Action->WorldContext = WorldContextObject;
	Action->FilePath = FilePath;
	Action->Data = Data;
	return Action;
}

void UAsyncAction_WriteFile::Activate()
{
	IGamingService* Service = ResolveService();
	ICloudStorageService* CloudStorage = Service ? Service->GetCloudStorage() : nullptr;
	if (!CloudStorage)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	CloudStorage->WriteFile(FilePath, Data, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_ReadFile* UAsyncAction_ReadFile::ReadFile(UObject* WorldContextObject, const FString& FilePath)
{
	UAsyncAction_ReadFile* Action = NewObject<UAsyncAction_ReadFile>();
	Action->WorldContext = WorldContextObject;
	Action->FilePath = FilePath;
	return Action;
}

void UAsyncAction_ReadFile::Activate()
{
	IGamingService* Service = ResolveService();
	ICloudStorageService* CloudStorage = Service ? Service->GetCloudStorage() : nullptr;
	if (!CloudStorage)
	{
		Completed.Broadcast(FFileReadResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	CloudStorage->ReadFile(FilePath, [this](const FFileReadResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_DeleteFile* UAsyncAction_DeleteFile::DeleteFile(UObject* WorldContextObject, const FString& FilePath)
{
	UAsyncAction_DeleteFile* Action = NewObject<UAsyncAction_DeleteFile>();
	Action->WorldContext = WorldContextObject;
	Action->FilePath = FilePath;
	return Action;
}

void UAsyncAction_DeleteFile::Activate()
{
	IGamingService* Service = ResolveService();
	ICloudStorageService* CloudStorage = Service ? Service->GetCloudStorage() : nullptr;
	if (!CloudStorage)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	CloudStorage->DeleteFile(FilePath, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_ListFiles* UAsyncAction_ListFiles::ListFiles(UObject* WorldContextObject, const FString& DirectoryPath)
{
	UAsyncAction_ListFiles* Action = NewObject<UAsyncAction_ListFiles>();
	Action->WorldContext = WorldContextObject;
	Action->DirectoryPath = DirectoryPath;
	return Action;
}

void UAsyncAction_ListFiles::Activate()
{
	IGamingService* Service = ResolveService();
	ICloudStorageService* CloudStorage = Service ? Service->GetCloudStorage() : nullptr;
	if (!CloudStorage)
	{
		Completed.Broadcast(FFilesListResult());
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	CloudStorage->ListFiles(DirectoryPath, [this](const FFilesListResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

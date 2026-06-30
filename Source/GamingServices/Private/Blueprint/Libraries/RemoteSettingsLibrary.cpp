#include "Blueprint/Libraries/RemoteSettingsLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IRemoteSettingsService.h"

UAsyncAction_SetRemoteSetting* UAsyncAction_SetRemoteSetting::SetRemoteSetting(UObject* WorldContextObject, const FString& Key, const FString& Value)
{
	UAsyncAction_SetRemoteSetting* Action = NewObject<UAsyncAction_SetRemoteSetting>();
	Action->WorldContext = WorldContextObject;
	Action->Key = Key;
	Action->Value = Value;
	return Action;
}

void UAsyncAction_SetRemoteSetting::Activate()
{
	IGamingService* Service = ResolveService();
	IRemoteSettingsService* RemoteSettings = Service ? Service->GetRemoteSettings() : nullptr;
	if (!RemoteSettings)
	{
		Completed.Broadcast(FRemoteSettingResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	RemoteSettings->SetRemoteSetting(Key, Value, [this](const FRemoteSettingResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_GetRemoteSetting* UAsyncAction_GetRemoteSetting::GetRemoteSetting(UObject* WorldContextObject, const FString& Key)
{
	UAsyncAction_GetRemoteSetting* Action = NewObject<UAsyncAction_GetRemoteSetting>();
	Action->WorldContext = WorldContextObject;
	Action->Key = Key;
	return Action;
}

void UAsyncAction_GetRemoteSetting::Activate()
{
	IGamingService* Service = ResolveService();
	IRemoteSettingsService* RemoteSettings = Service ? Service->GetRemoteSettings() : nullptr;
	if (!RemoteSettings)
	{
		Completed.Broadcast(FRemoteSettingResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	RemoteSettings->GetRemoteSetting(Key, [this](const FRemoteSettingResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_DeleteRemoteSetting* UAsyncAction_DeleteRemoteSetting::DeleteRemoteSetting(UObject* WorldContextObject, const FString& Key)
{
	UAsyncAction_DeleteRemoteSetting* Action = NewObject<UAsyncAction_DeleteRemoteSetting>();
	Action->WorldContext = WorldContextObject;
	Action->Key = Key;
	return Action;
}

void UAsyncAction_DeleteRemoteSetting::Activate()
{
	IGamingService* Service = ResolveService();
	IRemoteSettingsService* RemoteSettings = Service ? Service->GetRemoteSettings() : nullptr;
	if (!RemoteSettings)
	{
		Completed.Broadcast(FRemoteSettingResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	RemoteSettings->DeleteRemoteSetting(Key, [this](const FRemoteSettingResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_ListRemoteSettings* UAsyncAction_ListRemoteSettings::ListRemoteSettings(UObject* WorldContextObject)
{
	UAsyncAction_ListRemoteSettings* Action = NewObject<UAsyncAction_ListRemoteSettings>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_ListRemoteSettings::Activate()
{
	IGamingService* Service = ResolveService();
	IRemoteSettingsService* RemoteSettings = Service ? Service->GetRemoteSettings() : nullptr;
	if (!RemoteSettings)
	{
		Completed.Broadcast(FRemoteSettingsListResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	RemoteSettings->ListRemoteSettings([this](const FRemoteSettingsListResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RemoteSettingTypes.h"
#include "Native/GamingCapability.h"

/**
 * Key/value remote settings capability. Typically layered on top of CloudStorage via a single
 * settings blob; see FRemoteSettingsStore for the shared file-backed implementation.
 */
class GAMINGSERVICES_API IRemoteSettingsService
{
public:
	virtual ~IRemoteSettingsService() = default;

	virtual void SetRemoteSetting(const FString& Key, const FString& Value,
	                              TFunction<void(const FRemoteSettingResult&)> Callback) = 0;
	virtual void GetRemoteSetting(const FString& Key,
	                              TFunction<void(const FRemoteSettingResult&)> Callback) = 0;
	virtual void DeleteRemoteSetting(const FString& Key,
	                                 TFunction<void(const FRemoteSettingResult&)> Callback) = 0;
	virtual void ListRemoteSettings(TFunction<void(const FRemoteSettingsListResult&)> Callback) = 0;
};

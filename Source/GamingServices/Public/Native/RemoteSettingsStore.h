#pragma once

#include "CoreMinimal.h"
#include "Native/Interfaces/IRemoteSettingsService.h"

class ICloudStorageService;

/**
 * Backend-agnostic IRemoteSettingsService implemented on top of any ICloudStorageService.
 *
 * Settings are persisted as a single JSON blob (game_settings.json) in cloud storage, exactly as the
 * legacy FGamingService base did. Every real backend reuses this rather than re-implementing key/value
 * storage, so RemoteSettings support tracks CloudStorage support.
 */
class GAMINGSERVICES_API FRemoteSettingsStore final : public IRemoteSettingsService
{
public:
	/** CloudStorage must outlive this store (the owning service guarantees this). */
	explicit FRemoteSettingsStore(ICloudStorageService& InCloudStorage)
		: CloudStorage(InCloudStorage)
	{
	}

	virtual void SetRemoteSetting(const FString& Key, const FString& Value,
	                              TFunction<void(const FRemoteSettingResult&)> Callback) override;
	virtual void GetRemoteSetting(const FString& Key,
	                              TFunction<void(const FRemoteSettingResult&)> Callback) override;
	virtual void DeleteRemoteSetting(const FString& Key,
	                                 TFunction<void(const FRemoteSettingResult&)> Callback) override;
	virtual void ListRemoteSettings(TFunction<void(const FRemoteSettingsListResult&)> Callback) override;

private:
	static constexpr const TCHAR* SettingsFileName = TEXT("game_settings.json");

	static bool ParseSettingsFromBuffer(const TArray<uint8>& Buffer, TMap<FString, FString>& OutSettings);
	static bool SerializeSettingsToBuffer(const TMap<FString, FString>& Settings, TArray<uint8>& OutBuffer);

	ICloudStorageService& CloudStorage;
};

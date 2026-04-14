#include "Services/FGamingService.h"
#include "Misc/FileHelper.h"

bool FGamingService::ParseSettingsFromBuffer(const TArray<uint8>& Buffer, TMap<FString, FString>& OutSettings)
{
	if (Buffer.Num() == 0)
	{
		return false;
	}
	
	FString JsonString;
	FFileHelper::BufferToString(JsonString, Buffer.GetData(), Buffer.Num());
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}
	
	for (const auto& Pair : JsonObject->Values)
	{
		if (Pair.Value->Type == EJson::String)
		{
			OutSettings.Add(Pair.Key, Pair.Value->AsString());
		}
	}
	
	return true;
}

bool FGamingService::SerializeSettingsToBuffer(const TMap<FString, FString>& Settings, TArray<uint8>& OutBuffer)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	for (const auto& Pair : Settings)
	{
		JsonObject->SetStringField(Pair.Key, Pair.Value);
	}
	
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		return false;
	}
	
	OutBuffer.Append((uint8*)TCHAR_TO_UTF8(*JsonString), FCStringAnsi::Strlen(TCHAR_TO_UTF8(*JsonString)));
	return true;
}

void FGamingService::SetRemoteSetting(const FString& Key, const FString& Value,
                                      TFunction<void(const FRemoteSettingResult&)> Callback)
{
	UE_LOG(LogTemp, Log, TEXT("FGamingService: Setting remote setting %s=%s"), *Key, *Value);

	ReadFile(SettingsFileName, [this, Key, Value, Callback](const FFileReadResult& ReadResult)
	{
		TMap<FString, FString> Settings;
		
		if (ReadResult.bSuccess && ReadResult.Data.Num() > 0)
		{
			ParseSettingsFromBuffer(ReadResult.Data, Settings);
		}
		
		Settings.Add(Key, Value);
		
		TArray<uint8> JsonData;
		if (!SerializeSettingsToBuffer(Settings, JsonData))
		{
			UE_LOG(LogTemp, Error, TEXT("FGamingService: Failed to serialize settings for key: %s"), *Key);
			Callback(FRemoteSettingResult(false, Key, TEXT("")));
			return;
		}
		
		WriteFile(SettingsFileName, JsonData, [Key, Value, Callback](const FGamingServiceResult& WriteResult)
		{
			if (WriteResult.bSuccess)
			{
				Callback(FRemoteSettingResult(true, Key, Value));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("FGamingService: Failed to write settings for key: %s"), *Key);
				Callback(FRemoteSettingResult(false, Key, TEXT("")));
			}
		});
	});
}

void FGamingService::GetRemoteSetting(const FString& Key,
                                      TFunction<void(const FRemoteSettingResult&)> Callback)
{
	UE_LOG(LogTemp, Log, TEXT("FGamingService: Getting remote setting %s"), *Key);

	ReadFile(SettingsFileName, [Key, Callback](const FFileReadResult& ReadResult)
	{
		if (!ReadResult.bSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("FGamingService: Failed to read settings for key: %s"), *Key);
			Callback(FRemoteSettingResult(false, Key, TEXT("")));
			return;
		}
		
		TMap<FString, FString> Settings;
		if (!ParseSettingsFromBuffer(ReadResult.Data, Settings))
		{
			UE_LOG(LogTemp, Error, TEXT("FGamingService: Failed to parse settings for key: %s"), *Key);
			Callback(FRemoteSettingResult(false, Key, TEXT("")));
			return;
		}
		
		if (const FString* Value = Settings.Find(Key))
		{
			Callback(FRemoteSettingResult(true, Key, *Value));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("FGamingService: Setting not found for key: %s"), *Key);
			Callback(FRemoteSettingResult(false, Key, TEXT("")));
		}
	});
}

void FGamingService::DeleteRemoteSetting(const FString& Key,
                                         TFunction<void(const FRemoteSettingResult&)> Callback)
{
	UE_LOG(LogTemp, Log, TEXT("FGamingService: Deleting remote setting %s"), *Key);

	ReadFile(SettingsFileName, [this, Key, Callback](const FFileReadResult& ReadResult)
	{
		TMap<FString, FString> Settings;
		
		if (ReadResult.bSuccess && ReadResult.Data.Num() > 0)
		{
			ParseSettingsFromBuffer(ReadResult.Data, Settings);
		}
		
		if (!Settings.Contains(Key))
		{
			UE_LOG(LogTemp, Error, TEXT("FGamingService: Setting not found for key: %s"), *Key);
			Callback(FRemoteSettingResult(false, Key, TEXT("")));
			return;
		}
		
		Settings.Remove(Key);
		
		if (Settings.Num() == 0)
		{
			DeleteFile(SettingsFileName, [Key, Callback](const FGamingServiceResult& DeleteResult)
			{
				if (DeleteResult.bSuccess)
				{
					Callback(FRemoteSettingResult(true, Key, TEXT("")));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("FGamingService: Failed to delete settings file for key: %s"), *Key);
					Callback(FRemoteSettingResult(false, Key, TEXT("")));
				}
			});
		}
		else
		{
			TArray<uint8> JsonData;
			if (!SerializeSettingsToBuffer(Settings, JsonData))
			{
				UE_LOG(LogTemp, Error, TEXT("FGamingService: Failed to serialize settings for key: %s"), *Key);
				Callback(FRemoteSettingResult(false, Key, TEXT("")));
				return;
			}
			
			WriteFile(SettingsFileName, JsonData, [Key, Callback](const FGamingServiceResult& WriteResult)
			{
				if (WriteResult.bSuccess)
				{
					Callback(FRemoteSettingResult(true, Key, TEXT("")));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("FGamingService: Failed to write settings for key: %s"), *Key);
					Callback(FRemoteSettingResult(false, Key, TEXT("")));
				}
			});
		}
	});
}

void FGamingService::ListRemoteSettings(TFunction<void(const FRemoteSettingsListResult&)> Callback)
{
	UE_LOG(LogTemp, Log, TEXT("FGamingService: Listing remote settings"));

	ReadFile(SettingsFileName, [Callback](const FFileReadResult& ReadResult)
	{
		if (!ReadResult.bSuccess || ReadResult.Data.Num() == 0)
		{
			Callback(FRemoteSettingsListResult(true, TArray<FString>()));
			return;
		}
		
		TMap<FString, FString> Settings;
		if (!ParseSettingsFromBuffer(ReadResult.Data, Settings))
		{
			Callback(FRemoteSettingsListResult(true, TArray<FString>()));
			return;
		}
		
		TArray<FString> Keys;
		Settings.GetKeys(Keys);
		
		Callback(FRemoteSettingsListResult(true, Keys));
	});
}

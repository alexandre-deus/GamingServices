#pragma once
#include "CoreMinimal.h"
#include "GamingServiceTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class GAMINGSERVICES_API FGamingService
{
public:
	virtual ~FGamingService() = default;

	virtual bool Connect(const FGamingServiceConnectParams& Params) = 0;
	virtual void Shutdown() = 0;

	virtual void Login(const FGamingServiceLoginParams& Params,
	                   TFunction<void(const FGamingServiceResult&)> Callback) = 0;

	virtual void UnlockAchievement(const FString& AchievementId,
	                               TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryAchievements(TFunction<void(const FAchievementsQueryResult&)> Callback) = 0;
	virtual void WriteLeaderboardScore(const FString& LeaderboardId, int32 Score,
	                                   TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryLeaderboardPage(const FString& LeaderboardId, int32 Limit, int32 ContinuationToken,
	                                  TFunction<void(const FLeaderboardResult&)> Callback) = 0;

	virtual void IngestStat(const FString& StatName, int32 Amount,
	                        TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void QueryStat(const FString& StatName,
	                       TFunction<void(const FStatQueryResult&)> Callback) = 0;

	virtual void WriteFile(const FString& FilePath, const TArray<uint8>& Data,
	                       TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void ReadFile(const FString& FilePath,
	                      TFunction<void(const FFileReadResult&)> Callback) = 0;
	virtual void DeleteFile(const FString& FilePath,
	                        TFunction<void(const FGamingServiceResult&)> Callback) = 0;
	virtual void ListFiles(const FString& DirectoryPath,
	                       TFunction<void(const FFilesListResult&)> Callback) = 0;

	void SetRemoteSetting(const FString& Key, const FString& Value,
	                              TFunction<void(const FRemoteSettingResult&)> Callback);
	void GetRemoteSetting(const FString& Key,
	                              TFunction<void(const FRemoteSettingResult&)> Callback);
	void DeleteRemoteSetting(const FString& Key,
	                                 TFunction<void(const FRemoteSettingResult&)> Callback);
	void ListRemoteSettings(TFunction<void(const FRemoteSettingsListResult&)> Callback);

	virtual void Tick() = 0;

	virtual bool IsInitialized() const = 0;
	virtual bool IsLoggedIn() const = 0;
	virtual bool NeedsLogin() const = 0;
	virtual FString GetUserId() const = 0;
	virtual FString GetDisplayName() const = 0;
	template <class T>
	T& GetServiceAs() { return *static_cast<T*>(this); 	}

protected:
	static constexpr const TCHAR* SettingsFileName = TEXT("game_settings.json");
	
	static bool ParseSettingsFromBuffer(const TArray<uint8>& Buffer, TMap<FString, FString>& OutSettings);
	static bool SerializeSettingsToBuffer(const TMap<FString, FString>& Settings, TArray<uint8>& OutBuffer);
};

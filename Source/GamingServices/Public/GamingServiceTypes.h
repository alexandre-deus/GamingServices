#pragma once

#include "CoreMinimal.h"
#include "GamingServiceTypes.generated.h"

USTRUCT(BlueprintType)
struct FGameAchievement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	bool bIsUnlocked = false;

	UPROPERTY(BlueprintReadOnly)
	double Progress = 0.0;
};

USTRUCT(BlueprintType)
struct FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	FGamingServiceResult() = default;

	FGamingServiceResult(bool InSuccess) : bSuccess(InSuccess)
	{
	}
};

USTRUCT(BlueprintType)
struct FAchievementsQueryResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FGameAchievement> Achievements;

	FAchievementsQueryResult() = default;

	FAchievementsQueryResult(bool InSuccess,
	                         const TArray<FGameAchievement>& InAchievements = TArray<FGameAchievement>())
		: FGamingServiceResult(InSuccess)
		  , Achievements(InAchievements)
	{
	}
};

USTRUCT(BlueprintType)
struct FEntitlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString Description;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FEntitlementDefinition
{
	GENERATED_BODY()

	/** Logical name used to look up this entitlement at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName LogicalName;

	/** EOS catalogue entitlement name (e.g. "premium_dlc") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EOSEntitlementName;

	/** Steam DLC or App AppId */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SteamAppId = 0;
};

USTRUCT(BlueprintType)
struct FEntitlementsListResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FEntitlement> Entitlements;

	FEntitlementsListResult() = default;

	FEntitlementsListResult(bool InSuccess,
	                        const TArray<FEntitlement>& InEntitlements = TArray<FEntitlement>())
		: FGamingServiceResult(InSuccess)
		  , Entitlements(InEntitlements)
	{
	}
};

USTRUCT(BlueprintType)
struct FHasEntitlementResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString EntitlementId;

	UPROPERTY(BlueprintReadOnly)
	bool bHasEntitlement = false;

	FHasEntitlementResult() = default;

	FHasEntitlementResult(bool InSuccess, const FString& InEntitlementId = TEXT(""), bool bInHasEntitlement = false)
		: FGamingServiceResult(InSuccess)
		  , EntitlementId(InEntitlementId)
		  , bHasEntitlement(bInHasEntitlement)
	{
	}
};

USTRUCT(BlueprintType)
struct FLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString UserId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Rank = 0;

	FLeaderboardEntry() = default;

	FLeaderboardEntry(const FString& InUserId, const FString& InDisplayName, int32 InScore, int32 InRank)
		: UserId(InUserId), DisplayName(InDisplayName), Score(InScore), Rank(InRank)
	{
	}
};

USTRUCT(BlueprintType)
struct FLeaderboardResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString LeaderboardId;

	UPROPERTY(BlueprintReadOnly)
	TArray<FLeaderboardEntry> Entries;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalEntries = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ContinuationToken = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 UserRank = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 UserScore = 0;

	FLeaderboardResult() = default;

	FLeaderboardResult(bool InSuccess, const FString& InLeaderboardId = TEXT(""),
	                   const TArray<FLeaderboardEntry>& InEntries = TArray<FLeaderboardEntry>(),
	                   int32 InTotalEntries = 0, int32 InUserRank = -1, int32 InUserScore = 0)
		: FGamingServiceResult(InSuccess)
		  , LeaderboardId(InLeaderboardId), Entries(InEntries), TotalEntries(InTotalEntries)
		  , UserRank(InUserRank), UserScore(InUserScore)
	{
	}
};

USTRUCT(BlueprintType)
struct FStatQueryResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString StatName;

	UPROPERTY(BlueprintReadOnly)
	int32 Value = 0;

	FStatQueryResult() = default;

	static FStatQueryResult Make(const FString& InName, int32 InValue)
	{
		FStatQueryResult R;
		R.bSuccess = true;
		R.StatName = InName;
		R.Value = InValue;
		return R;
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FEOSInitOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ProductName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ProductVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ProductId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SandboxId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DeploymentId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ClientId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ClientSecret;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EncryptionKey;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSteamworksInitOptions
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FGamingServiceConnectParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEOSInitOptions EOS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSteamworksInitOptions Steamworks;
};

UENUM(BlueprintType)
enum class EEOSLoginMethod : uint8
{
	PersistentAuth UMETA(DisplayName = "Persistent Auth"),
	AccountPortal UMETA(DisplayName = "Account Portal"),
	DeviceCode UMETA(DisplayName = "Device Code"),
	Developer UMETA(DisplayName = "Developer")
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FEOSLoginOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEOSLoginMethod Method = EEOSLoginMethod::PersistentAuth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DeveloperHost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DeveloperCredentialName;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSteamworksLoginOptions
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FGamingServiceLoginParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEOSLoginOptions EOS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSteamworksLoginOptions Steamworks;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FFileBlobData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString FilePath;

	UPROPERTY(BlueprintReadOnly)
	int64 Size;

	UPROPERTY(BlueprintReadOnly)
	FDateTime LastModified;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FFilesListResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FFileBlobData> Files;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FFileReadResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString FilePath;

	TArray<uint8> Data;

	FFileReadResult() = default;

	FFileReadResult(bool InSuccess, const FString& InFilePath = TEXT(""), const TArray<uint8>& InData = TArray<uint8>())
		: FGamingServiceResult(InSuccess), FilePath(InFilePath), Data(InData)
	{
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FRemoteSettingResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Key;

	UPROPERTY(BlueprintReadOnly)
	FString Value;

	FRemoteSettingResult() = default;

	FRemoteSettingResult(bool InSuccess, const FString& InKey = TEXT(""), const FString& InValue = TEXT(""))
		: FGamingServiceResult(InSuccess), Key(InKey), Value(InValue)
	{
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FRemoteSettingsListResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Keys;

	FRemoteSettingsListResult() = default;

	FRemoteSettingsListResult(bool InSuccess, const TArray<FString>& InKeys = TArray<FString>())
		: FGamingServiceResult(InSuccess), Keys(InKeys)
	{
	}
};

// ============================================================================
// Matchmaking Types
// ============================================================================

class ISessionJoinHandle{};

USTRUCT(BlueprintType)
struct FSessionJoinHandle
{
	GENERATED_BODY()
	TSharedPtr<ISessionJoinHandle> BackendHandle;
};

UENUM(BlueprintType)
enum class ESessionPrivacy : uint8
{
	Public UMETA(DisplayName = "Public"),
	Private UMETA(DisplayName = "Private"),
	FriendsOnly UMETA(DisplayName = "Friends Only")
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Value;

	FSessionAttribute() = default;

	FSessionAttribute(const FString& InKey, const FString& InValue)
		: Key(InKey), Value(InValue)
	{
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SessionName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESessionPrivacy Privacy = ESessionPrivacy::Public;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowJoinInProgress = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUsesPresence = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowInvites = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAntiCheatProtected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSessionAttribute> CustomAttributes;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SessionName;

	UPROPERTY(BlueprintReadOnly)
	FString HostUserId;

	UPROPERTY(BlueprintReadOnly)
	FString HostDisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 4;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 AvailableSlots = 0;

	UPROPERTY(BlueprintReadOnly)
	ESessionPrivacy Privacy = ESessionPrivacy::Public;

	UPROPERTY(BlueprintReadOnly)
	bool bAllowJoinInProgress = true;

	UPROPERTY(BlueprintReadOnly)
	int32 Ping = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FSessionAttribute> CustomAttributes;

	UPROPERTY(BlueprintReadOnly)
	FSessionJoinHandle JoinHandle;

	FSessionInfo() = default;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionSearchFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxResults = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSessionAttribute> RequiredAttributes;

	FSessionSearchFilter() = default;
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionCreateResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FSessionInfo SessionInfo;

	FSessionCreateResult() = default;

	FSessionCreateResult(bool InSuccess, const FSessionInfo& InSessionInfo = FSessionInfo())
		: FGamingServiceResult(InSuccess), SessionInfo(InSessionInfo)
	{
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionSearchResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FSessionInfo> Sessions;

	FSessionSearchResult() = default;

	FSessionSearchResult(bool InSuccess, const TArray<FSessionInfo>& InSessions = TArray<FSessionInfo>())
		: FGamingServiceResult(InSuccess), Sessions(InSessions)
	{
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionJoinResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FSessionInfo SessionInfo;

	FSessionJoinResult() = default;

	FSessionJoinResult(bool InSuccess, const FSessionInfo& InSessionInfo = FSessionInfo())
		: FGamingServiceResult(InSuccess), SessionInfo(InSessionInfo)
	{
	}
};
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

	// EOS-specific options
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEOSInitOptions EOS;

	// Steamworks-specific options
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

	// Used only when Method == Developer
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DeveloperHost;

	// Used only when Method == Developer
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

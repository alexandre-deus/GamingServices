#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "SessionTypes.generated.h"

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

// Session events (host-side notifications).

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FSessionMemberInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString UserId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	FSessionMemberInfo() = default;

	FSessionMemberInfo(const FString& InUserId, const FString& InDisplayName)
		: UserId(InUserId), DisplayName(InDisplayName)
	{
	}
};

USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FLobbyInviteAcceptedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString InviterUserId;

	UPROPERTY(BlueprintReadOnly)
	FString InviterDisplayName;

	UPROPERTY(BlueprintReadOnly)
	FSessionJoinHandle JoinHandle;

	FLobbyInviteAcceptedInfo() = default;
};

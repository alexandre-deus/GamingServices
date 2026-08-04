#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "SessionTypes.generated.h"

/**
 * Opaque backend payload identifying a joinable session.
 *
 * Usually this wraps a live backend object obtained from a search or an invite. It can also carry
 * nothing but a lobby id — see FLobbyIdJoinHandle — which is what happens when the invite arrived
 * over a different platform than the one running the session.
 */
class ISessionJoinHandle
{
public:
	virtual ~ISessionJoinHandle() = default;

	/** Whether this handle owns a live backend object, as opposed to only naming the lobby. */
	virtual bool HasBackendDetails() const { return false; }

	/** Backend lobby id, when known. */
	virtual FString GetLobbyId() const { return FString(); }
};

/**
 * A join handle that is nothing more than a lobby id.
 *
 * Produced when the id reached us as text rather than as a backend object: a shared join code, or an
 * invite delivered through another platform's invite system (Steam carrying an EOS lobby id). The
 * backend resolves it by id at join time instead of joining through a details handle.
 */
struct GAMINGSERVICES_API FLobbyIdJoinHandle : public ISessionJoinHandle
{
	FString LobbyId;

	explicit FLobbyIdJoinHandle(const FString& InLobbyId) : LobbyId(InLobbyId) {}

	virtual FString GetLobbyId() const override { return LobbyId; }
};

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

/**
 * An invite that has arrived but has NOT been acted on yet.
 *
 * The counterpart to FLobbyInviteAcceptedInfo, which only ever describes an invite the player already
 * accepted somewhere else — a platform overlay. Backends without an overlay have to show the accept /
 * decline UI themselves, and that UI needs the invite before a decision exists.
 *
 * InviteId is the part FLobbyInviteAcceptedInfo has no reason to carry: it is what identifies the invite
 * back to the backend when declining, since a decline never produces a session to name instead.
 */
USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FLobbyInviteReceivedInfo
{
	GENERATED_BODY()

	/** Backend id for this specific invite. Required to reject it. */
	UPROPERTY(BlueprintReadOnly)
	FString InviteId;

	UPROPERTY(BlueprintReadOnly)
	FString InviterUserId;

	UPROPERTY(BlueprintReadOnly)
	FString InviterDisplayName;

	/**
	 * Ready to hand to JoinSession if the player accepts. Resolved up front because the backend can stop
	 * being able to resolve the invite id later, and because it keeps the accept path identical to the
	 * overlay one.
	 */
	UPROPERTY(BlueprintReadOnly)
	FSessionJoinHandle JoinHandle;

	FLobbyInviteReceivedInfo() = default;
};

/**
 * Every invite currently waiting on the local user.
 *
 * The notification sink only reports invites that arrive while the game is listening, so an invite sent
 * before launch — or while the platform layer was still signing in — is never announced. Polling is how
 * those are recovered; it is not a substitute for the sink, it covers the window the sink cannot.
 */
USTRUCT(BlueprintType)
struct GAMINGSERVICES_API FPendingInvitesResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FLobbyInviteReceivedInfo> Invites;

	FPendingInvitesResult() = default;

	explicit FPendingInvitesResult(bool bInSuccess) : FGamingServiceResult(bInSuccess)
	{
	}
};

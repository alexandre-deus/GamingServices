#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Blueprint/GamingServiceAsyncAction.h"
#include "DataTypes/SessionTypes.h"
#include "MatchmakingLibrary.generated.h"

// Completion pins (declared inline; one per distinct result type used in this file).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMatchmakingResultPin, const FGamingServiceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSessionCreatedPin, const FSessionCreateResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSessionsFoundPin, const FSessionSearchResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSessionJoinedPin, const FSessionJoinResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCurrentSessionPin, const FSessionInfo&, Info);

UCLASS()
class GAMINGSERVICES_API UAsyncAction_CreateSession : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FSessionCreatedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_CreateSession* CreateSession(UObject* WorldContextObject, const FSessionSettings& Settings);

	virtual void Activate() override;

private:
	UPROPERTY()
	FSessionSettings Settings;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_FindSessions : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FSessionsFoundPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_FindSessions* FindSessions(UObject* WorldContextObject, const FSessionSearchFilter& Filter);

	virtual void Activate() override;

private:
	UPROPERTY()
	FSessionSearchFilter Filter;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_JoinSession : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FSessionJoinedPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_JoinSession* JoinSession(UObject* WorldContextObject, const FSessionJoinHandle& JoinHandle);

	virtual void Activate() override;

private:
	FSessionJoinHandle JoinHandle;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_LeaveSession : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FMatchmakingResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_LeaveSession* LeaveSession(UObject* WorldContextObject);

	virtual void Activate() override;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_DestroySession : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FMatchmakingResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_DestroySession* DestroySession(UObject* WorldContextObject);

	virtual void Activate() override;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_UpdateSession : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FMatchmakingResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_UpdateSession* UpdateSession(UObject* WorldContextObject, const FSessionSettings& Settings);

	virtual void Activate() override;

private:
	UPROPERTY()
	FSessionSettings Settings;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_LockLobby : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FMatchmakingResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_LockLobby* LockLobby(UObject* WorldContextObject);

	virtual void Activate() override;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_UnlockLobby : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FMatchmakingResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_UnlockLobby* UnlockLobby(UObject* WorldContextObject);

	virtual void Activate() override;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_GetCurrentSession : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	/** GetCurrentSession always returns an FSessionInfo (no success flag), so it has a single result. */
	UPROPERTY(BlueprintAssignable)
	FCurrentSessionPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_GetCurrentSession* GetCurrentSession(UObject* WorldContextObject);

	virtual void Activate() override;
};

UCLASS()
class GAMINGSERVICES_API UAsyncAction_ShowInviteFriendsDialog : public UGamingServiceAsyncAction
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FMatchmakingResultPin Completed;

	UFUNCTION(BlueprintCallable, Category = "GamingServices|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UAsyncAction_ShowInviteFriendsDialog* ShowInviteFriendsDialog(UObject* WorldContextObject);

	virtual void Activate() override;
};

/** Synchronous matchmaking getters (no async work). */
UCLASS()
class GAMINGSERVICES_API UMatchmakingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "GamingServices|Matchmaking", meta = (WorldContext = "WorldContextObject"))
	static FString GetSessionConnectionString(const UObject* WorldContextObject);
};

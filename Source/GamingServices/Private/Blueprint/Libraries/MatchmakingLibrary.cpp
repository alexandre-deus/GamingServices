#include "Blueprint/Libraries/MatchmakingLibrary.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IMatchmakingService.h"
#include "Blueprint/GamingPlatformSubsystem.h"

UAsyncAction_CreateSession* UAsyncAction_CreateSession::CreateSession(UObject* WorldContextObject, const FSessionSettings& Settings)
{
	UAsyncAction_CreateSession* Action = NewObject<UAsyncAction_CreateSession>();
	Action->WorldContext = WorldContextObject;
	Action->Settings = Settings;
	return Action;
}

void UAsyncAction_CreateSession::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FSessionCreateResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->CreateSession(Settings, [this](const FSessionCreateResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_FindSessions* UAsyncAction_FindSessions::FindSessions(UObject* WorldContextObject, const FSessionSearchFilter& Filter)
{
	UAsyncAction_FindSessions* Action = NewObject<UAsyncAction_FindSessions>();
	Action->WorldContext = WorldContextObject;
	Action->Filter = Filter;
	return Action;
}

void UAsyncAction_FindSessions::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FSessionSearchResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->FindSessions(Filter, [this](const FSessionSearchResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_JoinSession* UAsyncAction_JoinSession::JoinSession(UObject* WorldContextObject, const FSessionJoinHandle& JoinHandle)
{
	UAsyncAction_JoinSession* Action = NewObject<UAsyncAction_JoinSession>();
	Action->WorldContext = WorldContextObject;
	Action->JoinHandle = JoinHandle;
	return Action;
}

void UAsyncAction_JoinSession::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FSessionJoinResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->JoinSession(JoinHandle, [this](const FSessionJoinResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_LeaveSession* UAsyncAction_LeaveSession::LeaveSession(UObject* WorldContextObject)
{
	UAsyncAction_LeaveSession* Action = NewObject<UAsyncAction_LeaveSession>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_LeaveSession::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->LeaveSession([this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_DestroySession* UAsyncAction_DestroySession::DestroySession(UObject* WorldContextObject)
{
	UAsyncAction_DestroySession* Action = NewObject<UAsyncAction_DestroySession>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_DestroySession::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->DestroySession([this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_UpdateSession* UAsyncAction_UpdateSession::UpdateSession(UObject* WorldContextObject, const FSessionSettings& Settings)
{
	UAsyncAction_UpdateSession* Action = NewObject<UAsyncAction_UpdateSession>();
	Action->WorldContext = WorldContextObject;
	Action->Settings = Settings;
	return Action;
}

void UAsyncAction_UpdateSession::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->UpdateSession(Settings, [this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_LockLobby* UAsyncAction_LockLobby::LockLobby(UObject* WorldContextObject)
{
	UAsyncAction_LockLobby* Action = NewObject<UAsyncAction_LockLobby>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_LockLobby::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->LockLobby([this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_UnlockLobby* UAsyncAction_UnlockLobby::UnlockLobby(UObject* WorldContextObject)
{
	UAsyncAction_UnlockLobby* Action = NewObject<UAsyncAction_UnlockLobby>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_UnlockLobby::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->UnlockLobby([this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

UAsyncAction_GetCurrentSession* UAsyncAction_GetCurrentSession::GetCurrentSession(UObject* WorldContextObject)
{
	UAsyncAction_GetCurrentSession* Action = NewObject<UAsyncAction_GetCurrentSession>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_GetCurrentSession::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FSessionInfo());
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->GetCurrentSession([this](const FSessionInfo& Info)
	{
		Completed.Broadcast(Info);
		SetReadyToDestroy();
	});
}

UAsyncAction_ShowInviteFriendsDialog* UAsyncAction_ShowInviteFriendsDialog::ShowInviteFriendsDialog(UObject* WorldContextObject)
{
	UAsyncAction_ShowInviteFriendsDialog* Action = NewObject<UAsyncAction_ShowInviteFriendsDialog>();
	Action->WorldContext = WorldContextObject;
	return Action;
}

void UAsyncAction_ShowInviteFriendsDialog::Activate()
{
	IGamingService* Service = ResolveService();
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	if (!Matchmaking)
	{
		Completed.Broadcast(FGamingServiceResult(false));
		SetReadyToDestroy();
		return;
	}

	KeepAlive();
	Matchmaking->ShowInviteFriendsDialog([this](const FGamingServiceResult& Result)
	{
		Completed.Broadcast(Result);
		SetReadyToDestroy();
	});
}

FString UMatchmakingLibrary::GetSessionConnectionString(const UObject* WorldContextObject)
{
	IGamingService* Service = UGamingPlatformSubsystem::GetServiceFromContext(WorldContextObject);
	IMatchmakingService* Matchmaking = Service ? Service->GetMatchmaking() : nullptr;
	return Matchmaking ? Matchmaking->GetSessionConnectionString() : FString();
}

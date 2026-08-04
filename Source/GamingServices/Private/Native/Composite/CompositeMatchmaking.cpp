#include "Native/Composite/CompositeMatchmaking.h"

#include "Native/Interfaces/IInviteTransport.h"
#include "Native/Steam/Interfaces/SteamInviteTransport.h"

namespace GamingServices
{
	FCompositeMatchmaking::FCompositeMatchmaking(IMatchmakingService& InInner, IInviteTransport* InInviteTransport)
		: Inner(InInner)
		, InviteTransport(InInviteTransport)
	{
		// Forward the inner backend's notifications through this object's own sinks, so callers bind here
		// and never need to know a decorator is in the way.
		Inner.OnSessionUserJoined = [this](const FSessionMemberInfo& Member)
		{
			if (OnSessionUserJoined) { OnSessionUserJoined(Member); }
		};
		Inner.OnSessionUserLeft = [this](const FSessionMemberInfo& Member)
		{
			if (OnSessionUserLeft) { OnSessionUserLeft(Member); }
		};
		Inner.OnSessionEnded = [this](const FGamingServiceResult& Result)
		{
			// The session is gone, so stop advertising it as joinable.
			WithdrawJoinInfo();
			if (OnSessionEnded) { OnSessionEnded(Result); }
		};
		Inner.OnLobbyInviteAccepted = [this](const FLobbyInviteAcceptedInfo& Info)
		{
			if (OnLobbyInviteAccepted) { OnLobbyInviteAccepted(Info); }
		};
		Inner.OnLobbyInviteReceived = [this](const FLobbyInviteReceivedInfo& Info)
		{
			// Forwarded even when a transport is present: the transport's platform can be down (game
			// launched outside the client), in which case the session backend's own invites are all there
			// is and the game has to show them itself. PlatformOwnsInviteUI() reports that same condition.
			if (OnLobbyInviteReceived) { OnLobbyInviteReceived(Info); }
		};

		if (InviteTransport)
		{
			// An invite accepted on the identity platform arrives as nothing but the primary's lobby id.
			// Wrap it as an id-only join handle and fire the ordinary invite-accepted sink, so it is
			// indistinguishable from a native invite to everything downstream.
			InviteTransport->OnJoinRequested = [this](const FString& JoinPayload)
			{
				if (JoinPayload.IsEmpty() || !OnLobbyInviteAccepted)
				{
					return;
				}

				UE_LOG(LogTemp, Log, TEXT("CompositeMatchmaking: invite accepted for lobby '%s'"), *JoinPayload);

				FLobbyInviteAcceptedInfo Info;
				Info.JoinHandle.BackendHandle = MakeShared<FLobbyIdJoinHandle>(JoinPayload);
				OnLobbyInviteAccepted(Info);
			};
		}
	}

	FCompositeMatchmaking::~FCompositeMatchmaking()
	{
		// The inner service and the transport outlive this decorator during teardown; drop the sinks so
		// nothing fires into a destroyed object.
		Inner.OnSessionUserJoined = nullptr;
		Inner.OnSessionUserLeft = nullptr;
		Inner.OnSessionEnded = nullptr;
		Inner.OnLobbyInviteAccepted = nullptr;
		Inner.OnLobbyInviteReceived = nullptr;
		if (InviteTransport)
		{
			InviteTransport->OnJoinRequested = nullptr;
		}
	}

	void FCompositeMatchmaking::PublishJoinInfo()
	{
		if (!InviteTransport)
		{
			return;
		}

		const FString LobbyId = Inner.GetCurrentLobbyId();
		if (LobbyId.IsEmpty())
		{
			InviteTransport->ClearJoinInfo();
			return;
		}
		InviteTransport->SetJoinInfo(LobbyId);
	}

	void FCompositeMatchmaking::WithdrawJoinInfo()
	{
		if (InviteTransport)
		{
			InviteTransport->ClearJoinInfo();
		}
	}

	void FCompositeMatchmaking::CreateSession(const FSessionSettings& Settings,
	                                          TFunction<void(const FSessionCreateResult&)> Callback)
	{
		Inner.CreateSession(Settings, [this, Callback = MoveTemp(Callback)](const FSessionCreateResult& Result) mutable
		{
			if (Result.bSuccess)
			{
				PublishJoinInfo();
			}
			if (Callback) { Callback(Result); }
		});
	}

	void FCompositeMatchmaking::JoinSession(const FSessionJoinHandle& JoinHandle,
	                                        TFunction<void(const FSessionJoinResult&)> Callback)
	{
		Inner.JoinSession(JoinHandle, [this, Callback = MoveTemp(Callback)](const FSessionJoinResult& Result) mutable
		{
			if (Result.bSuccess)
			{
				PublishJoinInfo();
			}
			if (Callback) { Callback(Result); }
		});
	}

	void FCompositeMatchmaking::JoinLobbyById(const FString& LobbyId,
	                                          TFunction<void(const FSessionJoinResult&)> Callback)
	{
		Inner.JoinLobbyById(LobbyId, [this, Callback = MoveTemp(Callback)](const FSessionJoinResult& Result) mutable
		{
			if (Result.bSuccess)
			{
				PublishJoinInfo();
			}
			if (Callback) { Callback(Result); }
		});
	}

	void FCompositeMatchmaking::LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		WithdrawJoinInfo();
		Inner.LeaveSession(MoveTemp(Callback));
	}

	void FCompositeMatchmaking::DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		WithdrawJoinInfo();
		Inner.DestroySession(MoveTemp(Callback));
	}

	void FCompositeMatchmaking::ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		// The player's friends are on the identity platform, not the one running the session, so its
		// overlay is the only picker that can show anyone. Fall back to the primary when there is no
		// transport (single-platform arrangements, or a transport that is not ready).
		if (InviteTransport && InviteTransport->IsAvailable())
		{
			InviteTransport->ShowInviteDialog(MoveTemp(Callback));
			return;
		}
		Inner.ShowInviteFriendsDialog(MoveTemp(Callback));
	}

	// --- Straight pass-through -------------------------------------------------------------------

	void FCompositeMatchmaking::FindSessions(const FSessionSearchFilter& Filter,
	                                         TFunction<void(const FSessionSearchResult&)> Callback)
	{
		Inner.FindSessions(Filter, MoveTemp(Callback));
	}

	void FCompositeMatchmaking::UpdateSession(const FSessionSettings& Settings,
	                                          TFunction<void(const FGamingServiceResult&)> Callback)
	{
		Inner.UpdateSession(Settings, MoveTemp(Callback));
	}

	void FCompositeMatchmaking::LockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		Inner.LockLobby(MoveTemp(Callback));
	}

	void FCompositeMatchmaking::UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		Inner.UnlockLobby(MoveTemp(Callback));
	}

	void FCompositeMatchmaking::GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
	{
		Inner.GetCurrentSession(MoveTemp(Callback));
	}

	FString FCompositeMatchmaking::GetCurrentLobbyId() const
	{
		return Inner.GetCurrentLobbyId();
	}

	bool FCompositeMatchmaking::PlatformOwnsInviteUI() const
	{
		// The identity platform owns the invite UI only while it is actually up. Launched outside that
		// client there is no overlay to defer to, the composite falls back to the session backend's own
		// login, and its invites are all the player will ever get - so the game has to draw them itself.
		// Answering from the transport rather than from the compiled profile is what makes that case work.
		if (InviteTransport && InviteTransport->IsAvailable())
		{
			return true;
		}

		return Inner.PlatformOwnsInviteUI();
	}

	void FCompositeMatchmaking::QueryPendingInvites(TFunction<void(const FPendingInvitesResult&)> Callback)
	{
		// Two different things were waiting, and they are not interchangeable.
		//
		// The transport may hold a payload from a launch-by-invite ("+connect"). The player ALREADY
		// accepted that one - accepting is what started the game - so it goes out through
		// OnLobbyInviteAccepted and must never appear in the list below, or they would be asked a question
		// they have already answered.
		//
		// The session backend holds invites nobody has answered yet. Those are the ones the caller gets
		// back, to put a prompt in front of.
		if (InviteTransport)
		{
			InviteTransport->FlushPendingJoin();
		}

		Inner.QueryPendingInvites(MoveTemp(Callback));
	}

	void FCompositeMatchmaking::RejectInvite(const FString& InviteId,
	                                          TFunction<void(const FGamingServiceResult&)> Callback)
	{
		// Always the session backend's business: the invite id came from its notification, and the
		// transport only ever carries an opaque payload it cannot interpret.
		Inner.RejectInvite(InviteId, MoveTemp(Callback));
	}

	FString FCompositeMatchmaking::GetSessionConnectionString() const
	{
		return Inner.GetSessionConnectionString();
	}
}

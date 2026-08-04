#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamMatchmaking.h"
#include "Native/Steam/Interfaces/SteamUser.h"
#include "Native/Steam/SteamPlatformCore.h"
#include "SteamCallResultManager.h"

#include "Misc/CommandLine.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	/** Steam-backed ISessionJoinHandle payload — wraps the lobby CSteamID. */
	struct FSteamworksSessionJoinHandle : public ISessionJoinHandle
	{
		CSteamID LobbyId;

		FSteamworksSessionJoinHandle(const CSteamID& InLobbyId)
			: LobbyId(InLobbyId) {}
	};

	/**
	 * When a user accepts a Steam lobby invite (or "Join Game") while the game is NOT running,
	 * Steam launches the game with "+connect_lobby <lobbyID>" on the command line instead of
	 * firing GameLobbyJoinRequested_t. Pull that lobby Steam ID out of the launch command line;
	 * returns 0 when no such token is present.
	 */
	static uint64 ParseConnectLobbyFromCommandLine()
	{
		const FString CmdLine = FCommandLine::Get();
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: [cmdline-join] full command line: '%s'"), *CmdLine);

		const TCHAR* Token = TEXT("+connect_lobby");
		const int32 TokenIndex = CmdLine.Find(Token, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (TokenIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: [cmdline-join] token '%s' not found — no cold-start lobby join"), Token);
			return 0;
		}
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: [cmdline-join] found token '%s' at index %d"), Token, TokenIndex);

		// Grab the first whitespace-delimited token after "+connect_lobby".
		FString Remainder = CmdLine.Mid(TokenIndex + FCString::Strlen(Token));
		Remainder.TrimStartInline();
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: [cmdline-join] text after token: '%s'"), *Remainder);

		FString LobbyIdStr;
		if (!Remainder.Split(TEXT(" "), &LobbyIdStr, nullptr))
		{
			LobbyIdStr = Remainder;
		}
		LobbyIdStr.TrimEndInline();

		const uint64 LobbyId = FCString::Strtoui64(*LobbyIdStr, nullptr, 10);
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: [cmdline-join] parsed lobby id token='%s' -> %llu"), *LobbyIdStr, LobbyId);
		if (LobbyId == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: [cmdline-join] lobby id token did not parse to a valid id"));
		}
		return LobbyId;
	}

	struct FSteamMatchmaking::FImpl
	{
		FSteamMatchmaking& Owner;
		FSteamPlatformCore& Core;
		FSteamUser& User;

		// ---- Lobby state ----
		CSteamID CurrentLobbyId;
		CSteamID CurrentHostId;
		bool bIsInLobby = false;
		bool bIsLobbyHost = false;
		TSet<uint64> CurrentLobbyMembers;

		// ---- Cold-start command-line lobby join ----
		// Parsed once from "+connect_lobby <id>"; held until a listener is bound, then delivered.
		CSteamID PendingCommandLineLobbyId;
		bool bCheckedCommandLine = false;
		bool bCommandLineJoinDelivered = false;

		// ---- Lobby search machinery ----
		struct FLobbySearchContext
		{
			TFunction<void(const FSessionSearchResult&)> Callback;
			TSet<uint64> PendingLobbyIDs; // Lobbies waiting for data
			TArray<CSteamID> LobbyIDs;     // All lobbies
			double StartTime = 0.0;        // When the search started
			double TimeoutSeconds = 5.0;   // Max time to wait for lobby data
		};

		TArray<TSharedPtr<FLobbySearchContext>> ActiveSearchContexts;

		CCallback<FImpl, LobbyChatUpdate_t> m_CallbackLobbyChatUpdate;
		CCallback<FImpl, GameLobbyJoinRequested_t> m_CallbackGameLobbyJoinRequested;

		FImpl(FSteamMatchmaking& InOwner, FSteamPlatformCore& InCore, FSteamUser& InUser)
			: Owner(InOwner)
			, Core(InCore)
			, User(InUser)
			, m_CallbackLobbyChatUpdate(this, &FImpl::OnLobbyChatUpdate)
			, m_CallbackGameLobbyJoinRequested(this, &FImpl::OnGameLobbyJoinRequested)
		{
		}

		void ProcessLobbySearchContexts()
		{
			if (ActiveSearchContexts.Num() == 0)
			{
				return;
			}

			double CurrentTime = FPlatformTime::Seconds();
			TArray<TSharedPtr<FLobbySearchContext>> CompletedContexts;

			for (TSharedPtr<FLobbySearchContext>& Context : ActiveSearchContexts)
			{
				if (!Context.IsValid())
				{
					CompletedContexts.Add(Context);
					continue;
				}

				// Check for timeout
				bool bTimedOut = (CurrentTime - Context->StartTime) > Context->TimeoutSeconds;
				if (bTimedOut && Context->PendingLobbyIDs.Num() > 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Lobby search timed out, %d lobbies still pending"), Context->PendingLobbyIDs.Num());
					// Continue anyway with whatever data we have
					Context->PendingLobbyIDs.Empty();
				}

				// Check each pending lobby to see if data is now available
				TArray<uint64> CompletedLobbies;
				for (uint64 LobbyID : Context->PendingLobbyIDs)
				{
					CSteamID SteamID(LobbyID);
					int32 DataCount = SteamMatchmaking()->GetLobbyDataCount(SteamID);

					// If we have data now, mark this lobby as complete
					if (DataCount > 0)
					{
						CompletedLobbies.Add(LobbyID);
						UE_LOG(LogTemp, Verbose, TEXT("SteamworksGamingService: Lobby %llu data ready (%d entries)"), LobbyID, DataCount);
					}
				}

				// Remove completed lobbies from pending set
				for (uint64 CompletedID : CompletedLobbies)
				{
					Context->PendingLobbyIDs.Remove(CompletedID);
				}

				// If all lobbies have data (or timed out), finalize this search
				if (Context->PendingLobbyIDs.Num() == 0)
				{
					UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: All lobby data received, finalizing search"));
					FinalizeLobbySearch(Context);
					CompletedContexts.Add(Context);
				}
			}

			// Remove completed contexts
			for (const TSharedPtr<FLobbySearchContext>& Completed : CompletedContexts)
			{
				ActiveSearchContexts.Remove(Completed);
			}
		}

		void FinalizeLobbySearch(TSharedPtr<FLobbySearchContext> SearchContext)
		{
			FSessionSearchResult FinalResult;
			FinalResult.bSuccess = true;

			for (const CSteamID& LobbyId : SearchContext->LobbyIDs)
			{
				FSessionInfo Session;
				Session.JoinHandle.BackendHandle = MakeShared<FSteamworksSessionJoinHandle>(LobbyId);

				const char* LobbyName = SteamMatchmaking()->GetLobbyData(LobbyId, "name");
				Session.SessionName = LobbyName;

				CSteamID OwnerID = SteamMatchmaking()->GetLobbyOwner(LobbyId);
				Session.HostUserId = FString::Printf(TEXT("%llu"), OwnerID.ConvertToUint64());
				Session.HostDisplayName = UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(OwnerID));

				Session.MaxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(LobbyId);
				Session.CurrentPlayers = SteamMatchmaking()->GetNumLobbyMembers(LobbyId);
				Session.AvailableSlots = Session.MaxPlayers - Session.CurrentPlayers;

				int32 DataCount = SteamMatchmaking()->GetLobbyDataCount(LobbyId);
				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby %llu | Name='%s' | Owner=%llu | Players=%d/%d | Metadata (%d entries):"),
					LobbyId.ConvertToUint64(),
					*Session.SessionName,
					OwnerID.ConvertToUint64(),
					Session.CurrentPlayers,
					Session.MaxPlayers,
					DataCount);

				for (int32 j = 0; j < DataCount; ++j)
				{
					char Key[256] = {0};
					char Value[256] = {0};
					if (SteamMatchmaking()->GetLobbyDataByIndex(LobbyId, j, Key, 256, Value, 256))
					{
						UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService:   [%d] '%s' = '%s'"), j, UTF8_TO_TCHAR(Key), UTF8_TO_TCHAR(Value));
						FSessionAttribute Attr;
						Attr.Key = UTF8_TO_TCHAR(Key);
						Attr.Value = UTF8_TO_TCHAR(Value);
						Session.CustomAttributes.Add(Attr);
					}
				}

				FinalResult.Sessions.Add(Session);
			}

			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Returning %d lobbies with metadata"), FinalResult.Sessions.Num());

			if (SearchContext->Callback)
			{
				SearchContext->Callback(FinalResult);
			}
		}

		void OnLobbyChatUpdate(LobbyChatUpdate_t* pParam)
		{
			if (!bIsInLobby || pParam->m_ulSteamIDLobby != CurrentLobbyId.ConvertToUint64())
			{
				return;
			}

			CSteamID ChangedUser(pParam->m_ulSteamIDUserChanged);
			// Id-only event, consistent across backends: the member's display name is resolved on
			// demand via IUserService::ResolveDisplayName(UserId), not carried in the event.
			FString ChangedUserId = FString::Printf(TEXT("%llu"), ChangedUser.ConvertToUint64());

			if (pParam->m_rgfChatMemberStateChange & k_EChatMemberStateChangeEntered)
			{
				CurrentLobbyMembers.Add(ChangedUser.ConvertToUint64());
				User.EnsureAvatarForMember(ChangedUser.ConvertToUint64());
				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: User joined lobby: %s"), *ChangedUserId);
				if (Owner.OnSessionUserJoined)
				{
					Owner.OnSessionUserJoined(FSessionMemberInfo(ChangedUserId, ChangedUserId));
				}
			}

			if (pParam->m_rgfChatMemberStateChange & (k_EChatMemberStateChangeLeft | k_EChatMemberStateChangeDisconnected | k_EChatMemberStateChangeKicked | k_EChatMemberStateChangeBanned))
			{
				CurrentLobbyMembers.Remove(ChangedUser.ConvertToUint64());
				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: User left lobby: %s"), *ChangedUserId);
				if (Owner.OnSessionUserLeft)
				{
					Owner.OnSessionUserLeft(FSessionMemberInfo(ChangedUserId, ChangedUserId));
				}

				// Two cases mean the lobby has effectively ended for us and our cached state must
				// be cleared, otherwise the next JoinSession/CreateSession takes a recursive
				// LeaveSession-then-rejoin path against dead Steam state and the rejoin races
				// Steam's P2P teardown.
				//   - The host left  -> the lobby is gone for everyone.
				//   - This user left -> kicked/banned/disconnected without going through our own
				//                       LeaveSession()/DestroySession() (Steam overlay quit, network drop, etc).
				const bool bHostLeft = (ChangedUser == CurrentHostId);
				const bool bSelfLeft = (SteamUser() && ChangedUser == SteamUser()->GetSteamID());
				if (bHostLeft || bSelfLeft)
				{
					if (bHostLeft)
					{
						UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Host has left the lobby"));
					}
					if (bSelfLeft)
					{
						UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Local user removed from lobby (kick/disconnect)"));
					}

					// Defensive: ask Steam to drop us from the lobby. No-op if Steam already
					// removed us (host-left tears the lobby down on Steam's side), but covers the
					// case where the chat-update reflects a partial state.
					if (SteamMatchmaking() && CurrentLobbyId.IsValid())
					{
						SteamMatchmaking()->LeaveLobby(CurrentLobbyId);
					}

					// Clear cached state before firing OnSessionEnded so application-layer
					// listeners that react by calling JoinSession/CreateSession see clean state.
					bIsInLobby = false;
					bIsLobbyHost = false;
					CurrentLobbyId = CSteamID();
					CurrentHostId = CSteamID();
					CurrentLobbyMembers.Empty();

					if (Owner.OnSessionEnded)
					{
						Owner.OnSessionEnded(FGamingServiceResult(true));
					}
				}
			}
		}

		void OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* pParam)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby invite accepted for lobby %llu"), pParam->m_steamIDLobby.ConvertToUint64());

			CSteamID FriendId = pParam->m_steamIDFriend;
			FString FriendUserId = FString::Printf(TEXT("%llu"), FriendId.ConvertToUint64());
			FString FriendDisplayName = SteamFriends() ? UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(FriendId)) : TEXT("Unknown");

			FLobbyInviteAcceptedInfo Info;
			Info.InviterUserId = FriendUserId;
			Info.InviterDisplayName = FriendDisplayName;
			Info.JoinHandle.BackendHandle = MakeShared<FSteamworksSessionJoinHandle>(pParam->m_steamIDLobby);

			if (Owner.OnLobbyInviteAccepted)
			{
				Owner.OnLobbyInviteAccepted(Info);
			}
		}

		/**
		 * Cold-start counterpart to OnGameLobbyJoinRequested: drives the "+connect_lobby" launch
		 * argument through the same OnLobbyInviteAccepted sink the in-game overlay path uses.
		 *
		 * Driven by QueryPendingInvites, not by Tick. It used to run every frame purely to discover when
		 * the application layer had finally bound its sink — the service ticks from the first frame, the
		 * game subscribes later, and firing eagerly would drop the join. An explicit "recover anything
		 * that was waiting" call is that signal, so the polling is gone.
		 *
		 * The command line is parsed once and the join delivered once. There is no inviter identity on a
		 * cold start, so those fields are left empty.
		 */
		void DeliverPendingCommandLineJoin()
		{
			if (bCommandLineJoinDelivered)
			{
				return;
			}

			if (!bCheckedCommandLine)
			{
				bCheckedCommandLine = true;
				const uint64 LobbyId = ParseConnectLobbyFromCommandLine();
				if (LobbyId != 0)
				{
					PendingCommandLineLobbyId = CSteamID(LobbyId);
					UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: [cmdline-join] cold-start join queued for lobby %llu"), LobbyId);
				}
			}

			if (!PendingCommandLineLobbyId.IsValid())
			{
				bCommandLineJoinDelivered = true;
				return;
			}

			// Deliberately does NOT latch as delivered: the caller asked too early, and a later call should
			// still find the join rather than having silently consumed it.
			if (!Owner.OnLobbyInviteAccepted)
			{
				UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: [cmdline-join] lobby %llu still pending — nothing is bound to OnLobbyInviteAccepted yet"), PendingCommandLineLobbyId.ConvertToUint64());
				return;
			}

			FLobbyInviteAcceptedInfo Info;
			Info.JoinHandle.BackendHandle = MakeShared<FSteamworksSessionJoinHandle>(PendingCommandLineLobbyId);

			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: [cmdline-join] delivering join for lobby %llu to OnLobbyInviteAccepted"), PendingCommandLineLobbyId.ConvertToUint64());
			Owner.OnLobbyInviteAccepted(Info);

			bCommandLineJoinDelivered = true;
			PendingCommandLineLobbyId = CSteamID();
		}
	};

	FSteamMatchmaking::FSteamMatchmaking(FSteamPlatformCore& InCore, FSteamUser& InUser)
		: Core(InCore)
		, User(InUser)
		, Impl(MakePimpl<FImpl>(*this, InCore, InUser))
	{
	}

	void FSteamMatchmaking::Tick()
	{
		Impl->ProcessLobbySearchContexts();
	}

	FString FSteamMatchmaking::GetSessionConnectionString() const
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		if (!Impl->bIsInLobby || !Impl->CurrentLobbyId.IsValid() || !SteamMatchmaking)
		{
			return FString();
		}

		CSteamID OwnerID = SteamMatchmaking->GetLobbyOwner(Impl->CurrentLobbyId);
		return FString::Printf(TEXT("steam.%llu"), OwnerID.ConvertToUint64());
	}

	FString FSteamMatchmaking::GetCurrentLobbyId() const
	{
		if (!Impl->bIsInLobby || !Impl->CurrentLobbyId.IsValid())
		{
			return FString();
		}
		return FString::Printf(TEXT("%llu"), Impl->CurrentLobbyId.ConvertToUint64());
	}

	void FSteamMatchmaking::CreateSession(const FSessionSettings& Settings, TFunction<void(const FSessionCreateResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: CreateSession called when service not ready"));

		if (Impl->bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Already in a lobby, leaving old lobby first"));
			DestroySession([this, Settings, Callback](const FGamingServiceResult& Result)
			{
				CreateSession(Settings, Callback);
			});
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Creating lobby: %s"), *Settings.SessionName);

		ELobbyType LobbyType = k_ELobbyTypePublic;
		if (Settings.Privacy == ESessionPrivacy::Private)
			LobbyType = k_ELobbyTypePrivate;
		else if (Settings.Privacy == ESessionPrivacy::FriendsOnly)
			LobbyType = k_ELobbyTypeFriendsOnly;

		SteamAPICall_t Handle = SteamMatchmaking->CreateLobby(LobbyType, Settings.MaxPlayers);

		Core.GetCallResults().Add<LobbyCreated_t>(Handle, [this, SteamMatchmaking, Settings, Callback](const LobbyCreated_t& Result, bool bIOFailure)
		{
			FSessionCreateResult CreateResult;
			CreateResult.bSuccess = !bIOFailure && Result.m_eResult == k_EResultOK;

			if (CreateResult.bSuccess)
			{
				Impl->CurrentLobbyId = CSteamID(Result.m_ulSteamIDLobby);
				Impl->bIsInLobby = true;
				Impl->bIsLobbyHost = true;

				// Snapshot current lobby members
				Impl->CurrentLobbyMembers.Empty();
				int32 MemberCount = SteamMatchmaking->GetNumLobbyMembers(Impl->CurrentLobbyId);
				for (int32 i = 0; i < MemberCount; ++i)
				{
					CSteamID Member = SteamMatchmaking->GetLobbyMemberByIndex(Impl->CurrentLobbyId, i);
					Impl->CurrentLobbyMembers.Add(Member.ConvertToUint64());
					User.EnsureAvatarForMember(Member.ConvertToUint64());
				}

				SteamMatchmaking->SetLobbyData(Impl->CurrentLobbyId, "name", TCHAR_TO_UTF8(*Settings.SessionName));

				for (const FSessionAttribute& Attr : Settings.CustomAttributes)
				{
					SteamMatchmaking->SetLobbyData(Impl->CurrentLobbyId, TCHAR_TO_UTF8(*Attr.Key), TCHAR_TO_UTF8(*Attr.Value));
				}

				CreateResult.SessionInfo.SessionName = Settings.SessionName;
				CreateResult.SessionInfo.HostUserId = Core.GetUserId();
				CreateResult.SessionInfo.HostDisplayName = Core.GetDisplayName();
				CreateResult.SessionInfo.MaxPlayers = Settings.MaxPlayers;
				CreateResult.SessionInfo.CurrentPlayers = 1;
				CreateResult.SessionInfo.AvailableSlots = Settings.MaxPlayers - 1;

				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby created successfully: %llu"), Impl->CurrentLobbyId.ConvertToUint64());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to create lobby"));
			}

			if (Callback)
			{
				Callback(CreateResult);
			}
		});
	}

	void FSteamMatchmaking::FindSessions(const FSessionSearchFilter& Filter, TFunction<void(const FSessionSearchResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: FindSessions called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Searching for lobbies, max results: %d"), Filter.MaxResults);

		SteamMatchmaking->AddRequestLobbyListResultCountFilter(Filter.MaxResults);

		for (const FSessionAttribute& Attr : Filter.RequiredAttributes)
		{
			SteamMatchmaking->AddRequestLobbyListStringFilter(TCHAR_TO_UTF8(*Attr.Key), TCHAR_TO_UTF8(*Attr.Value), k_ELobbyComparisonEqual);
		}

		SteamAPICall_t Handle = SteamMatchmaking->RequestLobbyList();

		TSharedPtr<FImpl::FLobbySearchContext> SearchContext = MakeShared<FImpl::FLobbySearchContext>();
		SearchContext->Callback = MoveTemp(Callback);
		SearchContext->StartTime = FPlatformTime::Seconds();

		ISteamMatchmaking* MatchmakingPtr = SteamMatchmaking;

		Core.GetCallResults().Add<LobbyMatchList_t>(Handle, [this, SearchContext, MatchmakingPtr](const LobbyMatchList_t& Result, bool bIOFailure)
		{
			if (bIOFailure)
			{
				UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Lobby search failed"));
				FSessionSearchResult ErrorResult;
				ErrorResult.bSuccess = false;
				if (SearchContext->Callback)
				{
					SearchContext->Callback(ErrorResult);
				}
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Found %d lobbies, requesting metadata..."), Result.m_nLobbiesMatching);

			if (Result.m_nLobbiesMatching == 0)
			{
				FSessionSearchResult EmptyResult;
				EmptyResult.bSuccess = true;
				if (SearchContext->Callback)
				{
					SearchContext->Callback(EmptyResult);
				}
				return;
			}

			// Collect lobby IDs
			for (uint32 i = 0; i < Result.m_nLobbiesMatching; ++i)
			{
				CSteamID LobbyId = MatchmakingPtr->GetLobbyByIndex(i);
				SearchContext->LobbyIDs.Add(LobbyId);
				SearchContext->PendingLobbyIDs.Add(LobbyId.ConvertToUint64());
			}

			// Request metadata for all lobbies (async, triggers LobbyDataUpdate_t callbacks)
			for (const CSteamID& LobbyId : SearchContext->LobbyIDs)
			{
				MatchmakingPtr->RequestLobbyData(LobbyId);
			}

			// Add to active contexts - will be processed in Tick() when data arrives
			Impl->ActiveSearchContexts.Add(SearchContext);

			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Waiting for %d lobby metadata updates..."), SearchContext->PendingLobbyIDs.Num());
		});
	}

	void FSteamMatchmaking::JoinLobbyById(const FString& LobbyId, TFunction<void(const FSessionJoinResult&)> Callback)
	{
		const CSteamID LobbySteamId(FCString::Strtoui64(*LobbyId, nullptr, 10));
		if (!LobbySteamId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: JoinLobbyById given invalid lobby id '%s'"), *LobbyId);
			Callback(FSessionJoinResult(false));
			return;
		}

		// A Steam lobby id is a shareable "join code"; reuse the normal join path with a handle built from it.
		FSessionJoinHandle JoinHandle;
		JoinHandle.BackendHandle = MakeShared<FSteamworksSessionJoinHandle>(LobbySteamId);
		JoinSession(JoinHandle, MoveTemp(Callback));
	}

	void FSteamMatchmaking::JoinSession(const FSessionJoinHandle& JoinHandle, TFunction<void(const FSessionJoinResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: JoinSession called when service not ready"));


		TSharedPtr<FSteamworksSessionJoinHandle> SteamworksHandle = StaticCastSharedPtr<FSteamworksSessionJoinHandle>(JoinHandle.BackendHandle);
		if (!SteamworksHandle->LobbyId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: JoinSession called with an invalid LobbyId."));
			FSessionJoinResult FailResult;
			FailResult.bSuccess = false;
			Callback(FailResult);
			return;
		}

		if (Impl->bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Already in a lobby, leaving old lobby first"));
			LeaveSession([this, JoinHandle, Callback](const FGamingServiceResult& Result)
			{
				JoinSession(JoinHandle, Callback);
			});
			return;
		}

		uint64 SteamIdUint64 = SteamworksHandle->LobbyId.ConvertToUint64();
		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Joining lobby: %llu"), SteamIdUint64);

		SteamAPICall_t Handle = SteamMatchmaking->JoinLobby(SteamworksHandle->LobbyId);

		Core.GetCallResults().Add<LobbyEnter_t>(Handle, [this, SteamMatchmaking, Callback](const LobbyEnter_t& Result, bool bIOFailure)
		{
			FSessionJoinResult JoinResult;
			JoinResult.bSuccess = !bIOFailure && (Result.m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess);

			if (JoinResult.bSuccess)
			{
				Impl->CurrentLobbyId = CSteamID(Result.m_ulSteamIDLobby);
				Impl->CurrentHostId = SteamMatchmaking->GetLobbyOwner(Impl->CurrentLobbyId);
				Impl->bIsInLobby = true;
				Impl->bIsLobbyHost = false;

				// Snapshot current lobby members
				Impl->CurrentLobbyMembers.Empty();
				int32 MemberCount = SteamMatchmaking->GetNumLobbyMembers(Impl->CurrentLobbyId);
				for (int32 i = 0; i < MemberCount; ++i)
				{
					CSteamID Member = SteamMatchmaking->GetLobbyMemberByIndex(Impl->CurrentLobbyId, i);
					Impl->CurrentLobbyMembers.Add(Member.ConvertToUint64());
					User.EnsureAvatarForMember(Member.ConvertToUint64());
				}

				const char* LobbyName = SteamMatchmaking->GetLobbyData(Impl->CurrentLobbyId, "name");
				JoinResult.SessionInfo.SessionName = UTF8_TO_TCHAR(LobbyName);

				CSteamID OwnerID = SteamMatchmaking->GetLobbyOwner(Impl->CurrentLobbyId);
				JoinResult.SessionInfo.HostUserId = FString::Printf(TEXT("%llu"), OwnerID.ConvertToUint64());
				JoinResult.SessionInfo.HostDisplayName = UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(OwnerID));

				JoinResult.SessionInfo.MaxPlayers = SteamMatchmaking->GetLobbyMemberLimit(Impl->CurrentLobbyId);
				JoinResult.SessionInfo.CurrentPlayers = SteamMatchmaking->GetNumLobbyMembers(Impl->CurrentLobbyId);
				JoinResult.SessionInfo.AvailableSlots = JoinResult.SessionInfo.MaxPlayers - JoinResult.SessionInfo.CurrentPlayers;

				UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Successfully joined lobby"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to join lobby"));
			}

			if (Callback)
			{
				Callback(JoinResult);
			}
		});
	}

	void FSteamMatchmaking::LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: LeaveSession called when service not ready"));

		if (!Impl->bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Not in a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Leaving lobby"));

		SteamMatchmaking->LeaveLobby(Impl->CurrentLobbyId);

		Impl->bIsInLobby = false;
		Impl->bIsLobbyHost = false;
		Impl->CurrentLobbyId = CSteamID();
		Impl->CurrentLobbyMembers.Empty();

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Successfully left lobby"));

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

	void FSteamMatchmaking::DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: DestroySession called when service not ready"));

		if (!Impl->bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamworksGamingService: Not in a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(true));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Destroying lobby"));

		SteamMatchmaking->LeaveLobby(Impl->CurrentLobbyId);

		Impl->bIsInLobby = false;
		Impl->bIsLobbyHost = false;
		Impl->CurrentLobbyId = CSteamID();
		Impl->CurrentLobbyMembers.Empty();

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Successfully destroyed lobby"));

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

	void FSteamMatchmaking::UpdateSession(const FSessionSettings& Settings, TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: UpdateSession called when service not ready"));

		if (!Impl->bIsInLobby || !Impl->bIsLobbyHost)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot update lobby - not hosting a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Updating lobby"));

		bool bSuccess = true;

		if (!Settings.SessionName.IsEmpty())
		{
			bSuccess &= SteamMatchmaking->SetLobbyData(Impl->CurrentLobbyId, "name", TCHAR_TO_UTF8(*Settings.SessionName));
		}

		if (Settings.MaxPlayers > 0)
		{
			bSuccess &= SteamMatchmaking->SetLobbyMemberLimit(Impl->CurrentLobbyId, Settings.MaxPlayers);
		}

		for (const FSessionAttribute& Attr : Settings.CustomAttributes)
		{
			bSuccess &= SteamMatchmaking->SetLobbyData(Impl->CurrentLobbyId, TCHAR_TO_UTF8(*Attr.Key), TCHAR_TO_UTF8(*Attr.Value));
		}

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby updated successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to update some lobby data"));
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void FSteamMatchmaking::LockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: LockLobby called when service not ready"));

		if (!Impl->bIsInLobby || !Impl->bIsLobbyHost)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot lock lobby - not hosting a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		const bool bSuccess = SteamMatchmaking->SetLobbyJoinable(Impl->CurrentLobbyId, false);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby locked successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to lock lobby"));
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void FSteamMatchmaking::UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamMatchmaking,
		       TEXT("SteamworksGamingService: UnlockLobby called when service not ready"));

		if (!Impl->bIsInLobby || !Impl->bIsLobbyHost)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot unlock lobby - not hosting a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		const bool bSuccess = SteamMatchmaking->SetLobbyJoinable(Impl->CurrentLobbyId, true);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Lobby unlocked successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Failed to unlock lobby"));
		}

		if (Callback)
		{
			Callback(FGamingServiceResult(bSuccess));
		}
	}

	void FSteamMatchmaking::GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
	{
		ISteamMatchmaking* SteamMatchmaking = ::SteamMatchmaking();
		FSessionInfo Info;

		if (Impl->bIsInLobby && Impl->CurrentLobbyId.IsValid())
		{
			const char* LobbyName = SteamMatchmaking->GetLobbyData(Impl->CurrentLobbyId, "name");
			Info.SessionName = LobbyName;

			CSteamID OwnerID = SteamMatchmaking->GetLobbyOwner(Impl->CurrentLobbyId);
			Info.HostUserId = FString::Printf(TEXT("%llu"), OwnerID.ConvertToUint64());
			Info.HostDisplayName = UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(OwnerID));

			Info.MaxPlayers = SteamMatchmaking->GetLobbyMemberLimit(Impl->CurrentLobbyId);
			Info.CurrentPlayers = SteamMatchmaking->GetNumLobbyMembers(Impl->CurrentLobbyId);
			Info.AvailableSlots = Info.MaxPlayers - Info.CurrentPlayers;
		}

		if (Callback)
		{
			Callback(Info);
		}
	}

	bool FSteamMatchmaking::PlatformOwnsInviteUI() const
	{
		return true;
	}

	void FSteamMatchmaking::RejectInvite(const FString& InviteId,
	                                     TFunction<void(const FGamingServiceResult&)> Callback)
	{
		UE_LOG(LogTemp, Log,
		       TEXT("SteamworksGamingService: invites are accepted or dismissed in the Steam overlay - "
		            "nothing to reject in-game"));

		if (Callback)
		{
			Callback(FGamingServiceResult(false));
		}
	}

	void FSteamMatchmaking::QueryPendingInvites(TFunction<void(const FPendingInvitesResult&)> Callback)
	{
		// A "+connect_lobby" cold start is an invite the player ALREADY accepted - accepting is what
		// launched the game - so it leaves through OnLobbyInviteAccepted and never appears in the list
		// below. This call is simply the point at which the game says it is ready to be handed one.
		Impl->DeliverPendingCommandLineJoin();

		// Not a gap to fill later: Steam has no API to enumerate UNANSWERED invites, because it never hands
		// the game one. The overlay holds those until the player acts.
		if (Callback)
		{
			Callback(FPendingInvitesResult(false));
		}
	}

	void FSteamMatchmaking::ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		ISteamFriends* SteamFriends = ::SteamFriends();
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && SteamFriends,
		       TEXT("SteamworksGamingService: ShowInviteFriendsDialog called when service not ready"));

		if (!Impl->bIsInLobby || !Impl->CurrentLobbyId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamworksGamingService: Cannot show invite dialog - not in a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamworksGamingService: Opening Steam invite overlay for lobby %llu"), Impl->CurrentLobbyId.ConvertToUint64());

		char LobbyIdStr[64];
		FCStringAnsi::Snprintf(LobbyIdStr, sizeof(LobbyIdStr), "%llu", Impl->CurrentLobbyId.ConvertToUint64());
		SteamFriends->ActivateGameOverlayInviteDialog(Impl->CurrentLobbyId);

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}
}

#endif // GS_WITH_STEAM

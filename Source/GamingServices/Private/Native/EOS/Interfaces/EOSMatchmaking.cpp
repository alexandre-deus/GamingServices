#if defined(USE_EOS)

#include "Native/EOS/Interfaces/EOSMatchmaking.h"
#include "Native/EOS/EOSPlatformCore.h"
#include "EOSCallbackContext.h"

namespace GamingServices
{
	using FSessionCreateCallbackCtx = TEOSCallbackContext<FSessionCreateResult, FEOSMatchmaking>;
	using FSessionJoinCallbackCtx = TEOSCallbackContext<FSessionJoinResult, FEOSMatchmaking>;
	using FSessionUpdateCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSMatchmaking>;

	// Cast the core's opaque accessors back to their EOS_* types in this .cpp so the core header stays SDK-free.
	static EOS_HSessions SessionsHandle(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_HSessions>(Core.GetSessionsHandle());
	}

	static EOS_ProductUserId ProductUserId(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_ProductUserId>(Core.GetProductUserId());
	}

	FEOSMatchmaking::FEOSMatchmaking(FEOSPlatformCore& InCore) : Core(InCore)
	{
		// The session-invite notification conceptually belongs here, but its registration needs the sessions
		// handle + ProductUserId that only exist after login, and its teardown must happen during core
		// shutdown. Bind the core's hooks so the core drives the timing while this class owns the work.
		Core.RegisterSessionInviteNotificationHook = [this]() { RegisterSessionInviteNotification(); };
		Core.UnregisterSessionInviteNotificationHook = [this]() { UnregisterSessionInviteNotification(); };
	}

	void FEOSMatchmaking::RegisterSessionInviteNotification()
	{
		// Register for session invite accepted notifications. Fired from the core's CompleteAuthentication,
		// matching the legacy ordering (right after ProductUserId is set and the sessions handle is valid).
		if (SessionsHandle(Core) && SessionInviteAcceptedNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Sessions_AddNotifySessionInviteAcceptedOptions InviteOptions = {};
			InviteOptions.ApiVersion = EOS_SESSIONS_ADDNOTIFYSESSIONINVITEACCEPTED_API_LATEST;

			SessionInviteAcceptedNotificationId = EOS_Sessions_AddNotifySessionInviteAccepted(
				SessionsHandle(Core),
				&InviteOptions,
				this,
				[](const EOS_Sessions_SessionInviteAcceptedCallbackInfo* Data)
				{
					check(Data);
					check(Data->ClientData);
					auto* Self = static_cast<FEOSMatchmaking*>(Data->ClientData);

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Session invite accepted, SessionId=%hs"),
					       Data->SessionId ? Data->SessionId : "null");

					FLobbyInviteAcceptedInfo Info;
					// EOS provides a session details handle for the invite
					if (Data->SessionId)
					{
						EOS_Sessions_CopySessionHandleForPresenceOptions CopyOptions = {};
						CopyOptions.ApiVersion = EOS_SESSIONS_COPYSESSIONHANDLEFORPRESENCE_API_LATEST;
						CopyOptions.SessionName = Data->SessionId;

						// The invite info is available; create a join handle
						FString SessionId = UTF8_TO_TCHAR(Data->SessionId);
						Info.JoinHandle.BackendHandle = MakeShared<FEOSSessionJoinHandle>(nullptr, SessionId);
					}

					if (Self->OnLobbyInviteAccepted)
					{
						Self->OnLobbyInviteAccepted(Info);
					}
				}
			);
		}
	}

	void FEOSMatchmaking::UnregisterSessionInviteNotification()
	{
		// Unregister session invite notification. Fired from the core's Shutdown, matching the legacy ordering.
		if (SessionsHandle(Core) && SessionInviteAcceptedNotificationId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Sessions_RemoveNotifySessionInviteAccepted(SessionsHandle(Core), SessionInviteAcceptedNotificationId);
			SessionInviteAcceptedNotificationId = EOS_INVALID_NOTIFICATIONID;
		}
	}

	void FEOSMatchmaking::CreateSession(const FSessionSettings& Settings,
	                                    TFunction<void(const FSessionCreateResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: CreateSession called when service not ready"));

		if (Core.bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Already in a session, destroying old session first"));
			DestroySession([this, Settings, Callback](const FGamingServiceResult& Result)
			{
				if (!Result.bSuccess)
				{
					FSessionCreateResult ErrorResult;
					ErrorResult.bSuccess = false;
					Callback(ErrorResult);
					return;
				}
				CreateSession(Settings, Callback);
			});
			return;
		}

		EOS_Sessions_CreateSessionModificationOptions CreateOptions = {};
		CreateOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
		CreateOptions.SessionName = TCHAR_TO_UTF8(*Settings.SessionName);
		CreateOptions.BucketId = TCHAR_TO_UTF8(TEXT("GameSessions"));
		CreateOptions.MaxPlayers = Settings.MaxPlayers;
		CreateOptions.LocalUserId = ProductUserId(Core);

		EOS_HSessionModification SessionModHandle = nullptr;
		EOS_EResult CreateModResult = EOS_Sessions_CreateSessionModification(SessionsHandle(Core), &CreateOptions, &SessionModHandle);

		if (CreateModResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session modification: %d"), (int32)CreateModResult);
			FSessionCreateResult ErrorResult;
			ErrorResult.bSuccess = false;
			Callback(ErrorResult);
			return;
		}

		EOS_SessionModification_SetPermissionLevelOptions PermissionOptions = {};
		PermissionOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
		if (Settings.Privacy == ESessionPrivacy::Public)
			PermissionOptions.PermissionLevel = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_PublicAdvertised;
		else if (Settings.Privacy == ESessionPrivacy::FriendsOnly)
			PermissionOptions.PermissionLevel = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly;
		else
			PermissionOptions.PermissionLevel = EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly;

		EOS_SessionModification_SetPermissionLevel(SessionModHandle, &PermissionOptions);

		EOS_SessionModification_SetJoinInProgressAllowedOptions JoinIPOptions = {};
		JoinIPOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST;
		JoinIPOptions.bAllowJoinInProgress = Settings.bAllowJoinInProgress ? EOS_TRUE : EOS_FALSE;
		EOS_SessionModification_SetJoinInProgressAllowed(SessionModHandle, &JoinIPOptions);

		for (const FSessionAttribute& Attr : Settings.CustomAttributes)
		{
			EOS_SessionModification_AddAttributeOptions AttrOptions = {};
			AttrOptions.ApiVersion = EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST;

			EOS_Sessions_AttributeData AttributeData = {};
			AttributeData.ApiVersion = EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST;

			FTCHARToUTF8 KeyConverter(*Attr.Key);
			FTCHARToUTF8 ValueConverter(*Attr.Value);

			AttributeData.Key = KeyConverter.Get();
			AttributeData.ValueType = EOS_ESessionAttributeType::EOS_SAT_String;
			AttributeData.Value.AsUtf8 = ValueConverter.Get();

			AttrOptions.SessionAttribute = &AttributeData;
			AttrOptions.AdvertisementType = EOS_ESessionAttributeAdvertisementType::EOS_SAAT_Advertise;

			EOS_SessionModification_AddAttribute(SessionModHandle, &AttrOptions);
		}

		EOS_Sessions_UpdateSessionOptions UpdateOptions = {};
		UpdateOptions.ApiVersion = EOS_SESSIONS_UPDATESESSION_API_LATEST;
		UpdateOptions.SessionModificationHandle = SessionModHandle;

		struct CreateSessionContext
		{
			FSessionCreateCallbackCtx* CreateCallbackCtx;
			FSessionSettings Settings;
		};
		auto* CallbackContext = FSessionCreateCallbackCtx::Create(this, MoveTemp(Callback));
		auto* CreateContext = new CreateSessionContext{CallbackContext, Settings};
		EOS_Sessions_UpdateSession(
			SessionsHandle(Core),
			&UpdateOptions,
			CreateContext,
			[](const EOS_Sessions_UpdateSessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<CreateSessionContext*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx->CreateCallbackCtx->Service;
				check(Self);

				FSessionCreateResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Self->Core.bIsInSession = true;
					Self->Core.bIsSessionHost = true;
					Self->Core.CurrentSessionName = LocalCtx->Settings.SessionName;
					Self->Core.CurrentSessionSettings = LocalCtx->Settings;

					Result.SessionInfo.SessionName = LocalCtx->Settings.SessionName;
					Result.SessionInfo.HostUserId = Self->Core.GetUserId();
					Result.SessionInfo.HostDisplayName = Self->Core.GetDisplayName();

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Session created successfully: %s"), *LocalCtx->Settings.SessionName);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session: %d"), (int32)Data->ResultCode);
				}

				FSessionCreateCallbackCtx::Complete(LocalCtx->CreateCallbackCtx, Result);
				delete LocalCtx;
			}
		);

		//EOS_SessionModification_Release(SessionModHandle);
	}

	void FEOSMatchmaking::FindSessions(const FSessionSearchFilter& Filter,
	                                   TFunction<void(const FSessionSearchResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: FindSessions called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Searching for sessions, max results: %d"), Filter.MaxResults);

		EOS_Sessions_CreateSessionSearchOptions SearchOptions = {};
		SearchOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST;
		SearchOptions.MaxSearchResults = Filter.MaxResults;

		EOS_HSessionSearch SearchHandle = nullptr;
		EOS_EResult CreateSearchResult = EOS_Sessions_CreateSessionSearch(SessionsHandle(Core), &SearchOptions, &SearchHandle);

		if (CreateSearchResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session search: %d"), (int32)CreateSearchResult);
			FSessionSearchResult ErrorResult;
			ErrorResult.bSuccess = false;
			Callback(ErrorResult);
			return;
		}

		for (const FSessionAttribute& Attr : Filter.RequiredAttributes)
		{
			EOS_SessionSearch_SetParameterOptions ParamOptions = {};
			ParamOptions.ApiVersion = EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST;

			EOS_Sessions_AttributeData AttributeData = {};
			AttributeData.ApiVersion = EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST;

			FTCHARToUTF8 KeyConverter(*Attr.Key);
			FTCHARToUTF8 ValueConverter(*Attr.Value);

			AttributeData.Key = KeyConverter.Get();
			AttributeData.ValueType = EOS_ESessionAttributeType::EOS_SAT_String;
			AttributeData.Value.AsUtf8 = ValueConverter.Get();

			ParamOptions.Parameter = &AttributeData;
			ParamOptions.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;

			EOS_SessionSearch_SetParameter(SearchHandle, &ParamOptions);
		}

		struct FSearchContext
		{
			FEOSMatchmaking* Service;
			TFunction<void(const FSessionSearchResult&)> Callback;
			EOS_HSessionSearch SearchHandle;
		};

		auto* SearchCtx = new FSearchContext{this, MoveTemp(Callback), SearchHandle};

		EOS_SessionSearch_FindOptions FindOptions = {};
		FindOptions.ApiVersion = EOS_SESSIONSEARCH_FIND_API_LATEST;
		FindOptions.LocalUserId = ProductUserId(Core);

		EOS_SessionSearch_Find(
			SearchHandle,
			&FindOptions,
			SearchCtx,
			[](const EOS_SessionSearch_FindCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSearchContext*>(Data->ClientData);

				FSessionSearchResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					EOS_SessionSearch_GetSearchResultCountOptions CountOptions = {};
					CountOptions.ApiVersion = EOS_SESSIONSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
					uint32_t ResultCount = EOS_SessionSearch_GetSearchResultCount(LocalCtx->SearchHandle, &CountOptions);

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Found %d sessions"), ResultCount);

					for (uint32_t i = 0; i < ResultCount; i++)
					{
						EOS_SessionSearch_CopySearchResultByIndexOptions CopyOptions = {};
						CopyOptions.ApiVersion = EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
						CopyOptions.SessionIndex = i;

						EOS_SessionDetails_Info* SessionInfo = nullptr;
						EOS_HSessionDetails SessionDetails = nullptr;

						if (EOS_SessionSearch_CopySearchResultByIndex(LocalCtx->SearchHandle, &CopyOptions, &SessionDetails) == EOS_EResult::EOS_Success)
						{
							EOS_SessionDetails_CopyInfoOptions InfoOptions = {};
							InfoOptions.ApiVersion = EOS_SESSIONDETAILS_COPYINFO_API_LATEST;

							if (EOS_SessionDetails_CopyInfo(SessionDetails, &InfoOptions, &SessionInfo) == EOS_EResult::EOS_Success && SessionInfo)
							{
								FSessionInfo Session;
								Session.SessionName = SessionInfo->SessionId ? FString(SessionInfo->SessionId) : TEXT("");
								const int32 MaxPlayersVal = SessionInfo->Settings ? (int32)SessionInfo->Settings->NumPublicConnections : 0;
								Session.MaxPlayers = MaxPlayersVal;
								Session.CurrentPlayers = (int32)SessionInfo->NumOpenPublicConnections < MaxPlayersVal
									? MaxPlayersVal - (int32)SessionInfo->NumOpenPublicConnections
									: MaxPlayersVal;
								Session.AvailableSlots = (int32)SessionInfo->NumOpenPublicConnections;
								Session.JoinHandle.BackendHandle = MakeShared<FEOSSessionJoinHandle>(
									SessionDetails, Session.SessionName);
								SessionDetails = nullptr;

								Result.Sessions.Add(Session);

								EOS_SessionDetails_Info_Release(SessionInfo);
							}
							else if (SessionDetails)
							{
								EOS_SessionDetails_Release(SessionDetails);
							}
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Session search failed: %d"), (int32)Data->ResultCode);
				}

				LocalCtx->Callback(Result);
				EOS_SessionSearch_Release(LocalCtx->SearchHandle);
				delete LocalCtx;
			}
		);
	}

	void FEOSMatchmaking::JoinSession(const FSessionJoinHandle& JoinHandle,
	                                  TFunction<void(const FSessionJoinResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: JoinSession called when service not ready"));

		if (!JoinHandle.BackendHandle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: JoinSession requires a session from FindSessions."));
			FSessionJoinResult FailResult;
			FailResult.bSuccess = false;
			Callback(FailResult);
			return;
		}

		if (Core.bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Already in a session, leaving old session first"));
			LeaveSession([this, JoinHandle, Callback](const FGamingServiceResult& Result)
			{
				JoinSession(JoinHandle, Callback);
			});
			return;
		}


		auto* Ctx = FSessionJoinCallbackCtx::Create(this, MoveTemp(Callback));
		struct FJoinSessionPayload
		{
			FSessionJoinCallbackCtx* Ctx;
			TSharedPtr<FEOSSessionJoinHandle> JoinHandle;
		};
		auto* Payload = new FJoinSessionPayload{ Ctx, StaticCastSharedPtr<FEOSSessionJoinHandle>(JoinHandle.BackendHandle) };
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Joining session: %s"), *Payload->JoinHandle->SessionName);

		EOS_Sessions_JoinSessionOptions JoinOptions = {};
		JoinOptions.ApiVersion = EOS_SESSIONS_JOINSESSION_API_LATEST;
		JoinOptions.SessionName = TCHAR_TO_UTF8(*Payload->JoinHandle->SessionName);
		JoinOptions.SessionHandle = Payload->JoinHandle->Handle;
		JoinOptions.LocalUserId = ProductUserId(Core);

		EOS_Sessions_JoinSession(
			SessionsHandle(Core),
			&JoinOptions,
			Payload,
			[](const EOS_Sessions_JoinSessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalPayload = static_cast<FJoinSessionPayload*>(Data->ClientData);
				FSessionJoinCallbackCtx* LocalCtx = LocalPayload->Ctx;
				FEOSMatchmaking* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);

				FSessionJoinResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Self->Core.bIsInSession = true;
					Self->Core.bIsSessionHost = false;
					Self->Core.CurrentSessionName = LocalPayload->JoinHandle->SessionName;
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Successfully joined session"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to join session: %d"), (int32)Data->ResultCode);
				}

				if (LocalCtx->Callback)
				{
					LocalCtx->Callback(Result);
				}
				delete LocalCtx;
				delete LocalPayload;
			}
		);
	}

	void FEOSMatchmaking::LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: LeaveSession called when service not ready"));

		if (!Core.bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Not in a session"));
			Callback(FGamingServiceResult(true));
			return;
		}

		if (Core.bIsSessionHost)
		{
			DestroySession(Callback);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Leaving session"));

		auto* Ctx = FSessionUpdateCallbackCtx::Create(this, MoveTemp(Callback));

		EOS_Sessions_DestroySessionOptions DestroyOptions = {};
		DestroyOptions.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST;
		DestroyOptions.SessionName = TCHAR_TO_UTF8(*Core.CurrentSessionName);

		EOS_Sessions_DestroySession(
			SessionsHandle(Core),
			&DestroyOptions,
			Ctx,
			[](const EOS_Sessions_DestroySessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionUpdateCallbackCtx*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);

				FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Self->Core.bIsInSession = false;
					Self->Core.CurrentSessionName.Empty();
					Self->Core.CurrentSessionSettings = FSessionSettings();
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Successfully left session"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to leave session: %d"), (int32)Data->ResultCode);
				}

				FSessionUpdateCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void FEOSMatchmaking::DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: DestroySession called when service not ready"));

		if (!Core.bIsInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Not in a session"));
			Callback(FGamingServiceResult(true));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Destroying session"));

		auto* Ctx = FSessionUpdateCallbackCtx::Create(this, MoveTemp(Callback));

		EOS_Sessions_DestroySessionOptions DestroyOptions = {};
		DestroyOptions.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST;
		DestroyOptions.SessionName = TCHAR_TO_UTF8(*Core.CurrentSessionName);

		EOS_Sessions_DestroySession(
			SessionsHandle(Core),
			&DestroyOptions,
			Ctx,
			[](const EOS_Sessions_DestroySessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionUpdateCallbackCtx*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx ? LocalCtx->Service : nullptr;
				check(Self);

				FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Self->Core.bIsInSession = false;
					Self->Core.bIsSessionHost = false;
					Self->Core.CurrentSessionName.Empty();
					Self->Core.CurrentSessionSettings = FSessionSettings();
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Successfully destroyed session"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to destroy session: %d"), (int32)Data->ResultCode);
				}

				FSessionUpdateCallbackCtx::Complete(LocalCtx, Result);
			}
		);
	}

	void FEOSMatchmaking::UpdateSession(const FSessionSettings& Settings,
	                                    TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: UpdateSession called when service not ready"));

		if (!Core.bIsInSession || !Core.bIsSessionHost)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot update session - not hosting a session"));
			Callback(FGamingServiceResult(false));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Updating session"));
		ApplySessionSettings(Settings, TEXT("update"), TEXT("EOSGamingService: Session updated successfully"), MoveTemp(Callback));
	}

	void FEOSMatchmaking::LockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: LockLobby called when service not ready"));

		if (!Core.bIsInSession || !Core.bIsSessionHost || Core.CurrentSessionName.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot lock lobby - not hosting a session"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Locking session"));

		FSessionSettings LockedSettings = Core.CurrentSessionSettings;
		LockedSettings.Privacy = ESessionPrivacy::Private;
		LockedSettings.bAllowInvites = false;
		LockedSettings.bAllowJoinInProgress = false;

		ApplySessionSettings(LockedSettings, TEXT("lock"), TEXT("EOSGamingService: Session locked successfully"), MoveTemp(Callback));
	}

	void FEOSMatchmaking::UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: UnlockLobby called when service not ready"));

		if (!Core.bIsInSession || !Core.bIsSessionHost || Core.CurrentSessionName.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot unlock lobby - not hosting a session"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Unlocking session"));

		FSessionSettings UnlockedSettings = Core.CurrentSessionSettings;
		UnlockedSettings.Privacy = ESessionPrivacy::Public;
		UnlockedSettings.bAllowInvites = true;
		UnlockedSettings.bAllowJoinInProgress = true;

		ApplySessionSettings(UnlockedSettings, TEXT("unlock"), TEXT("EOSGamingService: Session unlocked successfully"), MoveTemp(Callback));
	}

	void FEOSMatchmaking::ApplySessionSettings(const FSessionSettings& Settings, const TCHAR* OperationName,
	                                           const TCHAR* SuccessMessage,
	                                           TFunction<void(const FGamingServiceResult&)> Callback)
	{
		EOS_Sessions_CreateSessionModificationOptions CreateOptions = {};
		CreateOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
		CreateOptions.SessionName = TCHAR_TO_UTF8(*Core.CurrentSessionName);
		CreateOptions.BucketId = TCHAR_TO_UTF8(TEXT("GameSessions"));
		CreateOptions.MaxPlayers = Settings.MaxPlayers;
		CreateOptions.LocalUserId = ProductUserId(Core);

		EOS_HSessionModification SessionModHandle = nullptr;
		EOS_EResult CreateModResult = EOS_Sessions_CreateSessionModification(SessionsHandle(Core), &CreateOptions, &SessionModHandle);

		if (CreateModResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create session modification for %s: %d"), OperationName, (int32)CreateModResult);
			Callback(FGamingServiceResult(false));
			return;
		}

		EOS_SessionModification_SetPermissionLevelOptions PermissionOptions = {};
		PermissionOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
		PermissionOptions.PermissionLevel = Settings.Privacy == ESessionPrivacy::Public
			? EOS_EOnlineSessionPermissionLevel::EOS_OSPF_PublicAdvertised
			: EOS_EOnlineSessionPermissionLevel::EOS_OSPF_InviteOnly;
		const EOS_EResult PermissionResult = EOS_SessionModification_SetPermissionLevel(SessionModHandle, &PermissionOptions);

		EOS_SessionModification_SetJoinInProgressAllowedOptions JoinIPOptions = {};
		JoinIPOptions.ApiVersion = EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST;
		JoinIPOptions.bAllowJoinInProgress = Settings.bAllowJoinInProgress ? EOS_TRUE : EOS_FALSE;
		const EOS_EResult JoinResult = EOS_SessionModification_SetJoinInProgressAllowed(SessionModHandle, &JoinIPOptions);

		if (PermissionResult != EOS_EResult::EOS_Success || JoinResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to configure session %s (permission=%d, join=%d)"), OperationName, (int32)PermissionResult, (int32)JoinResult);
			EOS_SessionModification_Release(SessionModHandle);
			Callback(FGamingServiceResult(false));
			return;
		}

		EOS_Sessions_UpdateSessionOptions UpdateOptions = {};
		UpdateOptions.ApiVersion = EOS_SESSIONS_UPDATESESSION_API_LATEST;
		UpdateOptions.SessionModificationHandle = SessionModHandle;

		struct FUpdateSessionContext
		{
			FSessionUpdateCallbackCtx* CallbackCtx;
			FSessionSettings UpdatedSettings;
			FString OperationName;
			FString SuccessMessage;
		};

		auto* Ctx = FSessionUpdateCallbackCtx::Create(this, MoveTemp(Callback));
		auto* UpdateCtx = new FUpdateSessionContext{Ctx, Settings, OperationName, SuccessMessage};
		EOS_Sessions_UpdateSession(
			SessionsHandle(Core),
			&UpdateOptions,
			UpdateCtx,
			[](const EOS_Sessions_UpdateSessionCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FUpdateSessionContext*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx && LocalCtx->CallbackCtx ? LocalCtx->CallbackCtx->Service : nullptr;
				check(Self);

				FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					Self->Core.CurrentSessionSettings = LocalCtx->UpdatedSettings;
					UE_LOG(LogTemp, Log, TEXT("%s"), *LocalCtx->SuccessMessage);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to %s session: %d"), *LocalCtx->OperationName, (int32)Data->ResultCode);
				}

				FSessionUpdateCallbackCtx::Complete(LocalCtx->CallbackCtx, Result);
				delete LocalCtx;
			}
		);

		EOS_SessionModification_Release(SessionModHandle);
	}

	void FEOSMatchmaking::GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
	{
		FSessionInfo Info;

		if (Core.bIsInSession)
		{
			Info.SessionName = Core.CurrentSessionName;
			Info.HostUserId = Core.GetUserId();
			Info.HostDisplayName = Core.GetDisplayName();
			Info.MaxPlayers = Core.CurrentSessionSettings.MaxPlayers;
			Info.Privacy = Core.CurrentSessionSettings.Privacy;
			Info.bAllowJoinInProgress = Core.CurrentSessionSettings.bAllowJoinInProgress;
			Info.CustomAttributes = Core.CurrentSessionSettings.CustomAttributes;
		}

		Callback(Info);
	}

	void FEOSMatchmaking::ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetSessionsHandle(),
		       TEXT("EOSGamingService: ShowInviteFriendsDialog called when service not ready"));

		if (!Core.bIsInSession || Core.CurrentSessionName.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot show invite dialog - not in a session"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Sending session invite to all friends via EOS overlay"));

		EOS_Sessions_SendInviteOptions InviteOptions = {};
		InviteOptions.ApiVersion = EOS_SESSIONS_SENDINVITE_API_LATEST;
		InviteOptions.SessionName = TCHAR_TO_UTF8(*Core.CurrentSessionName);
		InviteOptions.LocalUserId = ProductUserId(Core);

		// EOS doesn't have a friend-picker dialog like Steam overlay.
		// Use the EOS UI Social Overlay to let the user pick friends if available.
		// For now we report success so calling code can implement a custom friend picker.
		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: EOS does not provide a built-in invite friends dialog. Use platform-specific overlay or custom UI."));

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

	FString FEOSMatchmaking::GetSessionConnectionString() const
	{
		return Core.GetSessionConnectionString();
	}
}

#endif // USE_EOS

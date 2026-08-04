#if defined(GS_WITH_EOS)

#include "Native/EOS/Interfaces/EOSMatchmaking.h"
#include "EOSCommon.h"
#include "EOSCallbackContext.h"

#include <string>

namespace GamingServices
{
	/** EOS-backed ISessionJoinHandle payload — owns the lobby EOS_HLobbyDetails and releases it on destruction. */
	struct FEOSSessionJoinHandle : public ISessionJoinHandle
	{
		EOS_HLobbyDetails Handle = nullptr;
		FString LobbyId;
		FString SessionName;

		FEOSSessionJoinHandle(EOS_HLobbyDetails InHandle, const FString& InLobbyId, const FString& InSessionName)
			: Handle(InHandle), LobbyId(InLobbyId), SessionName(InSessionName) {}

		virtual bool HasBackendDetails() const override { return Handle != nullptr; }
		virtual FString GetLobbyId() const override { return LobbyId; }

		~FEOSSessionJoinHandle()
		{
			if (Handle)
			{
				EOS_LobbyDetails_Release(Handle);
				Handle = nullptr;
			}
		}
	};

	using FSessionCreateCallbackCtx = TEOSCallbackContext<FSessionCreateResult, FEOSMatchmaking>;
	using FSessionJoinCallbackCtx = TEOSCallbackContext<FSessionJoinResult, FEOSMatchmaking>;
	using FSessionUpdateCallbackCtx = TEOSCallbackContext<FGamingServiceResult, FEOSMatchmaking>;

	// The lobby bucket every game lobby lives in, and the attribute keys used to carry the
	// human-facing session name (lobby attribute) and each member's display name (member attribute).
	static const char* GLobbyBucketId = "GameSessions";
	static const char* GSessionNameAttributeKey = "GS_SESSIONNAME";
	static const char* GDisplayNameAttributeKey = "GS_DISPLAYNAME";

	// Cast the core's opaque accessors back to their EOS_* types in this .cpp so the core header stays SDK-free.
	static EOS_HLobby LobbyHandle(const FEOSPlatformCore& Core)
	{
		return static_cast<EOS_HLobby>(Core.GetLobbyHandle());
	}

	static EOS_ELobbyPermissionLevel ToLobbyPermission(ESessionPrivacy Privacy)
	{
		switch (Privacy)
		{
		case ESessionPrivacy::Public: return EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
		case ESessionPrivacy::FriendsOnly: return EOS_ELobbyPermissionLevel::EOS_LPL_JOINVIAPRESENCE;
		default: return EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY;
		}
	}

	static ESessionPrivacy FromLobbyPermission(EOS_ELobbyPermissionLevel Permission)
	{
		switch (Permission)
		{
		case EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED: return ESessionPrivacy::Public;
		case EOS_ELobbyPermissionLevel::EOS_LPL_JOINVIAPRESENCE: return ESessionPrivacy::FriendsOnly;
		default: return ESessionPrivacy::Private;
		}
	}

	/** Read a string lobby attribute; empty when missing or not a string. */
	static FString GetLobbyAttribute(EOS_HLobbyDetails Details, const char* Key)
	{
		EOS_LobbyDetails_CopyAttributeByKeyOptions Options = {};
		Options.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;
		Options.AttrKey = Key;

		FString Value;
		EOS_Lobby_Attribute* Attribute = nullptr;
		if (EOS_LobbyDetails_CopyAttributeByKey(Details, &Options, &Attribute) == EOS_EResult::EOS_Success && Attribute)
		{
			if (Attribute->Data && Attribute->Data->ValueType == EOS_EAttributeType::EOS_AT_STRING &&
				Attribute->Data->Value.AsUtf8)
			{
				Value = UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8);
			}
			EOS_Lobby_Attribute_Release(Attribute);
		}
		return Value;
	}

	/** Read a member's string attribute (e.g. their advertised display name). */
	static FString GetMemberAttribute(EOS_HLobbyDetails Details, EOS_ProductUserId Member, const char* Key)
	{
		EOS_LobbyDetails_CopyMemberAttributeByKeyOptions Options = {};
		Options.ApiVersion = EOS_LOBBYDETAILS_COPYMEMBERATTRIBUTEBYKEY_API_LATEST;
		Options.TargetUserId = Member;
		Options.AttrKey = Key;

		FString Value;
		EOS_Lobby_Attribute* Attribute = nullptr;
		if (EOS_LobbyDetails_CopyMemberAttributeByKey(Details, &Options, &Attribute) == EOS_EResult::EOS_Success &&
			Attribute)
		{
			if (Attribute->Data && Attribute->Data->ValueType == EOS_EAttributeType::EOS_AT_STRING &&
				Attribute->Data->Value.AsUtf8)
			{
				Value = UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8);
			}
			EOS_Lobby_Attribute_Release(Attribute);
		}
		return Value;
	}

	/**
	 * Turn an invite id into everything the game needs to show and act on it. Shared by the arrival
	 * notification and the pending-invite poll, which differ only in where the id came from.
	 *
	 * Returns false when the invite no longer resolves to a lobby — the host closed it, or it was already
	 * consumed elsewhere — in which case the invite is not worth offering, because accepting could not
	 * work. On success OutInfo's join handle owns the details handle and releases it when dropped.
	 */
	static bool BuildInviteInfo(const FEOSPlatformCore& Core, const char* InviteId, EOS_ProductUserId Sender,
	                            FLobbyInviteReceivedInfo& OutInfo)
	{
		if (!InviteId)
		{
			return false;
		}

		OutInfo.InviteId = UTF8_TO_TCHAR(InviteId);

		EOS_Lobby_CopyLobbyDetailsHandleByInviteIdOptions CopyOptions = {};
		CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLEBYINVITEID_API_LATEST;
		CopyOptions.InviteId = InviteId;

		EOS_HLobbyDetails Details = nullptr;
		if (EOS_Lobby_CopyLobbyDetailsHandleByInviteId(LobbyHandle(Core), &CopyOptions, &Details) !=
			EOS_EResult::EOS_Success || !Details)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("EOSGamingService: invite %hs no longer resolves to a lobby - ignoring"), InviteId);
			return false;
		}

		// The arrival notification names the sender; the pending-invite cache does not. Fall back to the
		// lobby owner, who is the person being joined either way, so a polled invite is not nameless.
		EOS_ProductUserId Inviter = Sender;
		if (!Inviter)
		{
			EOS_LobbyDetails_GetLobbyOwnerOptions OwnerOptions = {};
			OwnerOptions.ApiVersion = EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
			Inviter = EOS_LobbyDetails_GetLobbyOwner(Details, &OwnerOptions);
		}

		// Neither invite path carries a LobbyId, so leave it empty: the handle owns live details, which is
		// what JoinSession uses; the id would only appear in logging.
		const FString SessionName = GetLobbyAttribute(Details, GSessionNameAttributeKey);
		OutInfo.InviterUserId = PuidToString(Inviter);
		OutInfo.InviterDisplayName = Inviter
			                             ? GetMemberAttribute(Details, Inviter, GDisplayNameAttributeKey)
			                             : FString();
		OutInfo.JoinHandle.BackendHandle = MakeShared<FEOSSessionJoinHandle>(Details, FString(), SessionName);
		return true;
	}

	/** Copy the details handle of the lobby this user is currently in; nullptr when unavailable. */
	static EOS_HLobbyDetails CopyCurrentLobbyDetails(const FEOSPlatformCore& Core, const FString& LobbyId)
	{
		const std::string LobbyIdUtf8 = TCHAR_TO_UTF8(*LobbyId);

		EOS_Lobby_CopyLobbyDetailsHandleOptions Options = {};
		Options.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
		Options.LobbyId = LobbyIdUtf8.c_str();
		Options.LocalUserId = ProductUserId(Core);

		EOS_HLobbyDetails Details = nullptr;
		if (EOS_Lobby_CopyLobbyDetailsHandle(LobbyHandle(Core), &Options, &Details) != EOS_EResult::EOS_Success)
		{
			return nullptr;
		}
		return Details;
	}

	FEOSMatchmaking::FEOSMatchmaking(FEOSPlatformCore& InCore) : Core(InCore)
	{
		// The lobby notifications conceptually belong here, but their registration needs the lobby handle +
		// ProductUserId that only exist after login, and their teardown must happen during core shutdown.
		// Bind the core's hooks so the core drives the timing while this class owns the work.
		Core.RegisterMatchmakingNotificationsHook = [this]() { RegisterLobbyNotifications(); };
		Core.UnregisterMatchmakingNotificationsHook = [this]() { UnregisterLobbyNotifications(); };
	}

	void FEOSMatchmaking::ResetLobbyState()
	{
		bIsInLobby = false;
		bIsLobbyOwner = false;
		CurrentLobbyId.Empty();
		CurrentLobbyOwnerPuid.Empty();
		CurrentSessionName.Empty();
		CurrentSessionSettings = FSessionSettings();
	}

	void FEOSMatchmaking::RegisterLobbyNotifications()
	{
		if (!LobbyHandle(Core))
		{
			return;
		}

		if (LobbyInviteAcceptedNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Lobby_AddNotifyLobbyInviteAcceptedOptions InviteOptions = {};
			InviteOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYINVITEACCEPTED_API_LATEST;

			LobbyInviteAcceptedNotificationId = EOS_Lobby_AddNotifyLobbyInviteAccepted(
				LobbyHandle(Core),
				&InviteOptions,
				this,
				[](const EOS_Lobby_LobbyInviteAcceptedCallbackInfo* Data)
				{
					check(Data);
					check(Data->ClientData);
					auto* Self = static_cast<FEOSMatchmaking*>(Data->ClientData);

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Lobby invite accepted, LobbyId=%hs"),
					       Data->LobbyId ? Data->LobbyId : "null");

					FLobbyInviteAcceptedInfo Info;
					Info.InviterUserId = PuidToString(Data->TargetUserId);

					EOS_Lobby_CopyLobbyDetailsHandleByInviteIdOptions CopyOptions = {};
					CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLEBYINVITEID_API_LATEST;
					CopyOptions.InviteId = Data->InviteId;

					EOS_HLobbyDetails Details = nullptr;
					if (EOS_Lobby_CopyLobbyDetailsHandleByInviteId(LobbyHandle(Self->Core), &CopyOptions, &Details) ==
						EOS_EResult::EOS_Success && Details)
					{
						const FString LobbyId = Data->LobbyId ? UTF8_TO_TCHAR(Data->LobbyId) : FString();
						const FString SessionName = GetLobbyAttribute(Details, GSessionNameAttributeKey);
						Info.InviterDisplayName = GetMemberAttribute(
							Details, Data->TargetUserId, GDisplayNameAttributeKey);
						Info.JoinHandle.BackendHandle = MakeShared<FEOSSessionJoinHandle>(Details, LobbyId, SessionName);
					}

					if (Self->OnLobbyInviteAccepted)
					{
						Self->OnLobbyInviteAccepted(Info);
					}
				});
		}

		if (LobbyInviteReceivedNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Lobby_AddNotifyLobbyInviteReceivedOptions ReceivedOptions = {};
			ReceivedOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYINVITERECEIVED_API_LATEST;

			LobbyInviteReceivedNotificationId = EOS_Lobby_AddNotifyLobbyInviteReceived(
				LobbyHandle(Core),
				&ReceivedOptions,
				this,
				[](const EOS_Lobby_LobbyInviteReceivedCallbackInfo* Data)
				{
					check(Data);
					check(Data->ClientData);
					auto* Self = static_cast<FEOSMatchmaking*>(Data->ClientData);

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Lobby invite received, InviteId=%hs"),
					       Data->InviteId ? Data->InviteId : "null");

					FLobbyInviteReceivedInfo Info;
					if (!BuildInviteInfo(Self->Core, Data->InviteId, Data->TargetUserId, Info))
					{
						// Without details there is nothing to join, so surfacing a toast would offer an accept
						// that cannot work. Drop it rather than show a dead prompt.
						return;
					}

					if (Self->OnLobbyInviteReceived)
					{
						Self->OnLobbyInviteReceived(Info);
					}
				});

			// Worth stating either way: an invite that never arrives is otherwise indistinguishable from
			// one that was never sent, and this is the line that tells the two apart.
			if (LobbyInviteReceivedNotificationId == EOS_INVALID_NOTIFICATIONID)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("EOSGamingService: FAILED to register the lobby invite-received notification - "
				            "incoming invites will not be seen"));
			}
			else
			{
				UE_LOG(LogTemp, Log,
				       TEXT("EOSGamingService: listening for incoming lobby invites (notification %llu)"),
				       LobbyInviteReceivedNotificationId);
			}
		}

		if (LobbyMemberStatusNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions StatusOptions = {};
			StatusOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;

			LobbyMemberStatusNotificationId = EOS_Lobby_AddNotifyLobbyMemberStatusReceived(
				LobbyHandle(Core),
				&StatusOptions,
				this,
				[](const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* Data)
				{
					check(Data);
					check(Data->ClientData);
					auto* Self = static_cast<FEOSMatchmaking*>(Data->ClientData);

					const FString LobbyId = Data->LobbyId ? UTF8_TO_TCHAR(Data->LobbyId) : FString();
					if (!Self->bIsInLobby || LobbyId != Self->CurrentLobbyId)
					{
						return;
					}

					const FString MemberPuid = PuidToString(Data->TargetUserId);
					const bool bIsSelf = MemberPuid == Self->Core.GetUserId();

					switch (Data->CurrentStatus)
					{
					case EOS_ELobbyMemberStatus::EOS_LMS_JOINED:
						if (!bIsSelf)
						{
							// Id-only event, consistent across backends: the member's display name is
							// resolved on demand via IUserService::ResolveDisplayName(UserId). Reading the
							// name here would race anyway — the joiner advertises it one RTT after this fires.
							UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Lobby member joined: %s"), *MemberPuid);
							if (Self->OnSessionUserJoined)
							{
								Self->OnSessionUserJoined(FSessionMemberInfo(MemberPuid, MemberPuid));
							}
						}
						break;

					case EOS_ELobbyMemberStatus::EOS_LMS_LEFT:
					case EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED:
					case EOS_ELobbyMemberStatus::EOS_LMS_KICKED:
						if (!bIsSelf)
						{
							UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Lobby member left: %s"), *MemberPuid);
							if (Self->OnSessionUserLeft)
							{
								// The member is already gone from the lobby, so their advertised display
								// name is no longer readable; the id is the reliable field here.
								Self->OnSessionUserLeft(FSessionMemberInfo(MemberPuid, MemberPuid));
							}
						}
						else
						{
							// We were kicked (or dropped) from the lobby.
							UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Removed from lobby %s"), *LobbyId);
							Self->ResetLobbyState();
							if (Self->OnSessionEnded)
							{
								Self->OnSessionEnded(FGamingServiceResult(true));
							}
						}
						break;

					case EOS_ELobbyMemberStatus::EOS_LMS_CLOSED:
						// The lobby was destroyed (host migration is disabled, so this is the host leaving).
						UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Lobby %s closed"), *LobbyId);
						Self->ResetLobbyState();
						if (Self->OnSessionEnded)
						{
							Self->OnSessionEnded(FGamingServiceResult(true));
						}
						break;

					case EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED:
						Self->CurrentLobbyOwnerPuid = MemberPuid;
						Self->bIsLobbyOwner = bIsSelf;
						break;

					default:
						break;
					}
				});
		}
	}

	void FEOSMatchmaking::UnregisterLobbyNotifications()
	{
		if (!LobbyHandle(Core))
		{
			return;
		}
		if (LobbyInviteAcceptedNotificationId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Lobby_RemoveNotifyLobbyInviteAccepted(LobbyHandle(Core), LobbyInviteAcceptedNotificationId);
			LobbyInviteAcceptedNotificationId = EOS_INVALID_NOTIFICATIONID;
		}
		if (LobbyInviteReceivedNotificationId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Lobby_RemoveNotifyLobbyInviteReceived(LobbyHandle(Core), LobbyInviteReceivedNotificationId);
			LobbyInviteReceivedNotificationId = EOS_INVALID_NOTIFICATIONID;
		}
		if (LobbyMemberStatusNotificationId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(LobbyHandle(Core), LobbyMemberStatusNotificationId);
			LobbyMemberStatusNotificationId = EOS_INVALID_NOTIFICATIONID;
		}
	}

	void FEOSMatchmaking::CreateSession(const FSessionSettings& Settings,
	                                    TFunction<void(const FSessionCreateResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: CreateSession called when service not ready"));

		if (bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Already in a lobby, leaving old lobby first"));
			LeaveSession([this, Settings, Callback](const FGamingServiceResult& Result)
			{
				if (!Result.bSuccess)
				{
					Callback(FSessionCreateResult(false));
					return;
				}
				CreateSession(Settings, Callback);
			});
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Creating lobby '%s'"), *Settings.SessionName);

		EOS_Lobby_CreateLobbyOptions CreateOptions = {};
		CreateOptions.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
		CreateOptions.LocalUserId = ProductUserId(Core);
		CreateOptions.MaxLobbyMembers = Settings.MaxPlayers;
		CreateOptions.PermissionLevel = ToLobbyPermission(Settings.Privacy);
		CreateOptions.bPresenceEnabled = Settings.bUsesPresence ? EOS_TRUE : EOS_FALSE;
		CreateOptions.bAllowInvites = Settings.bAllowInvites ? EOS_TRUE : EOS_FALSE;
		// Required for EOS_Lobby_JoinLobbyById, which is the only way into a lobby that is not
		// discoverable by search. It is what makes "invisible to everyone, joinable by whoever was given
		// the id" possible, and is exactly the case the SDK documents it for: an integrated platform's
		// invite system (Steam) carrying the lobby id to the invited player.
		CreateOptions.bEnableJoinById = EOS_TRUE;
		CreateOptions.BucketId = GLobbyBucketId;
		// Host leaving destroys the lobby, matching the previous Sessions semantics: members receive
		// EOS_LMS_CLOSED and the OnSessionEnded sink fires.
		CreateOptions.bDisableHostMigration = EOS_TRUE;

		struct FCreateLobbyCtx
		{
			FSessionCreateCallbackCtx* Ctx;
			FSessionSettings Settings;
		};
		auto* CreateCtx = new FCreateLobbyCtx{FSessionCreateCallbackCtx::Create(this, MoveTemp(Callback)), Settings};

		EOS_Lobby_CreateLobby(
			LobbyHandle(Core),
			&CreateOptions,
			CreateCtx,
			[](const EOS_Lobby_CreateLobbyCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FCreateLobbyCtx> LocalCtx(static_cast<FCreateLobbyCtx*>(Data->ClientData));
				FEOSMatchmaking* Self = LocalCtx->Ctx->Service;
				check(Self);

				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create lobby: %d"), (int32)Data->ResultCode);
					FSessionCreateCallbackCtx::Complete(LocalCtx->Ctx, FSessionCreateResult(false));
					return;
				}

				Self->bIsInLobby = true;
				Self->bIsLobbyOwner = true;
				Self->CurrentLobbyId = Data->LobbyId ? UTF8_TO_TCHAR(Data->LobbyId) : FString();
				Self->CurrentLobbyOwnerPuid = Self->Core.GetUserId();
				Self->CurrentSessionName = LocalCtx->Settings.SessionName;
				Self->CurrentSessionSettings = LocalCtx->Settings;

				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Lobby created: %s"), *Self->CurrentLobbyId);

				// Publish the session-name / custom attributes and this member's display name, then
				// complete the create with the final result.
				FSessionCreateCallbackCtx* CompletionCtx = LocalCtx->Ctx;
				Self->ApplySessionSettings(
					LocalCtx->Settings, TEXT("create"),
					TEXT("EOSGamingService: Lobby attributes published"),
					[Self, CompletionCtx](const FGamingServiceResult& AttrResult)
					{
						FSessionCreateResult Result(AttrResult.bSuccess);
						if (AttrResult.bSuccess)
						{
							Result.SessionInfo.SessionName = Self->CurrentSessionName;
							Result.SessionInfo.HostUserId = Self->Core.GetUserId();
							Result.SessionInfo.HostDisplayName = Self->Core.GetDisplayName();
							Result.SessionInfo.MaxPlayers = Self->CurrentSessionSettings.MaxPlayers;
							Result.SessionInfo.Privacy = Self->CurrentSessionSettings.Privacy;
							Result.SessionInfo.CustomAttributes = Self->CurrentSessionSettings.CustomAttributes;
						}
						else
						{
							UE_LOG(LogTemp, Error,
							       TEXT("EOSGamingService: Lobby created but attribute publish failed; destroying"));
							Self->DestroySession([](const FGamingServiceResult&) {});
						}
						FSessionCreateCallbackCtx::Complete(CompletionCtx, Result);
					});
			});
	}

	void FEOSMatchmaking::FindSessions(const FSessionSearchFilter& Filter,
	                                   TFunction<void(const FSessionSearchResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: FindSessions called when service not ready"));

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Searching for lobbies, max results: %d"), Filter.MaxResults);

		EOS_Lobby_CreateLobbySearchOptions SearchOptions = {};
		SearchOptions.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
		SearchOptions.MaxResults = FMath::Max(1, Filter.MaxResults);

		EOS_HLobbySearch SearchHandle = nullptr;
		const EOS_EResult CreateSearchResult = EOS_Lobby_CreateLobbySearch(LobbyHandle(Core), &SearchOptions, &SearchHandle);
		if (CreateSearchResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create lobby search: %d"), (int32)CreateSearchResult);
			Callback(FSessionSearchResult(false));
			return;
		}

		// Always constrain to our bucket; add every required attribute as an equality parameter.
		{
			EOS_Lobby_AttributeData BucketData = {};
			BucketData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
			BucketData.Key = EOS_LOBBY_SEARCH_BUCKET_ID;
			BucketData.ValueType = EOS_EAttributeType::EOS_AT_STRING;
			BucketData.Value.AsUtf8 = GLobbyBucketId;

			EOS_LobbySearch_SetParameterOptions BucketParam = {};
			BucketParam.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
			BucketParam.Parameter = &BucketData;
			BucketParam.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
			EOS_LobbySearch_SetParameter(SearchHandle, &BucketParam);
		}

		for (const FSessionAttribute& Attr : Filter.RequiredAttributes)
		{
			const FTCHARToUTF8 KeyUtf8(*Attr.Key);
			const FTCHARToUTF8 ValueUtf8(*Attr.Value);

			EOS_Lobby_AttributeData AttributeData = {};
			AttributeData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
			AttributeData.Key = KeyUtf8.Get();
			AttributeData.ValueType = EOS_EAttributeType::EOS_AT_STRING;
			AttributeData.Value.AsUtf8 = ValueUtf8.Get();

			EOS_LobbySearch_SetParameterOptions ParamOptions = {};
			ParamOptions.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
			ParamOptions.Parameter = &AttributeData;
			ParamOptions.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
			EOS_LobbySearch_SetParameter(SearchHandle, &ParamOptions);
		}

		struct FSearchContext
		{
			FEOSMatchmaking* Service;
			TFunction<void(const FSessionSearchResult&)> Callback;
			EOS_HLobbySearch SearchHandle;
		};
		auto* SearchCtx = new FSearchContext{this, MoveTemp(Callback), SearchHandle};

		EOS_LobbySearch_FindOptions FindOptions = {};
		FindOptions.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
		FindOptions.LocalUserId = ProductUserId(Core);

		EOS_LobbySearch_Find(
			SearchHandle,
			&FindOptions,
			SearchCtx,
			[](const EOS_LobbySearch_FindCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FSearchContext> LocalCtx(static_cast<FSearchContext*>(Data->ClientData));

				FSessionSearchResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

				if (Result.bSuccess)
				{
					EOS_LobbySearch_GetSearchResultCountOptions CountOptions = {};
					CountOptions.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
					const uint32_t ResultCount = EOS_LobbySearch_GetSearchResultCount(LocalCtx->SearchHandle, &CountOptions);

					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Found %d lobbies"), ResultCount);

					for (uint32_t i = 0; i < ResultCount; i++)
					{
						EOS_LobbySearch_CopySearchResultByIndexOptions CopyOptions = {};
						CopyOptions.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
						CopyOptions.LobbyIndex = i;

						EOS_HLobbyDetails Details = nullptr;
						if (EOS_LobbySearch_CopySearchResultByIndex(LocalCtx->SearchHandle, &CopyOptions, &Details) !=
							EOS_EResult::EOS_Success || !Details)
						{
							continue;
						}

						EOS_LobbyDetails_CopyInfoOptions InfoOptions = {};
						InfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;

						EOS_LobbyDetails_Info* Info = nullptr;
						if (EOS_LobbyDetails_CopyInfo(Details, &InfoOptions, &Info) != EOS_EResult::EOS_Success || !Info)
						{
							EOS_LobbyDetails_Release(Details);
							continue;
						}

						EOS_LobbyDetails_GetLobbyOwnerOptions OwnerOptions = {};
						OwnerOptions.ApiVersion = EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
						const EOS_ProductUserId Owner = EOS_LobbyDetails_GetLobbyOwner(Details, &OwnerOptions);

						FSessionInfo Session;
						const FString LobbyId = Info->LobbyId ? UTF8_TO_TCHAR(Info->LobbyId) : FString();
						Session.SessionName = GetLobbyAttribute(Details, GSessionNameAttributeKey);
						if (Session.SessionName.IsEmpty())
						{
							Session.SessionName = LobbyId;
						}
						Session.HostUserId = PuidToString(Owner);
						Session.HostDisplayName = GetMemberAttribute(Details, Owner, GDisplayNameAttributeKey);
						Session.MaxPlayers = (int32)Info->MaxMembers;
						Session.AvailableSlots = (int32)Info->AvailableSlots;
						Session.CurrentPlayers = Session.MaxPlayers - Session.AvailableSlots;
						Session.Privacy = FromLobbyPermission(Info->PermissionLevel);

						// Surface every public string attribute except our internal session-name carrier.
						EOS_LobbyDetails_GetAttributeCountOptions AttrCountOptions = {};
						AttrCountOptions.ApiVersion = EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST;
						const uint32_t AttrCount = EOS_LobbyDetails_GetAttributeCount(Details, &AttrCountOptions);
						for (uint32_t AttrIndex = 0; AttrIndex < AttrCount; ++AttrIndex)
						{
							EOS_LobbyDetails_CopyAttributeByIndexOptions AttrOptions = {};
							AttrOptions.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST;
							AttrOptions.AttrIndex = AttrIndex;

							EOS_Lobby_Attribute* Attribute = nullptr;
							if (EOS_LobbyDetails_CopyAttributeByIndex(Details, &AttrOptions, &Attribute) ==
								EOS_EResult::EOS_Success && Attribute)
							{
								if (Attribute->Data && Attribute->Data->Key &&
									Attribute->Data->ValueType == EOS_EAttributeType::EOS_AT_STRING &&
									Attribute->Data->Value.AsUtf8 &&
									FCStringAnsi::Strcmp(Attribute->Data->Key, GSessionNameAttributeKey) != 0)
								{
									Session.CustomAttributes.Add(FSessionAttribute(
										UTF8_TO_TCHAR(Attribute->Data->Key),
										UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8)));
								}
								EOS_Lobby_Attribute_Release(Attribute);
							}
						}

						// The join handle owns the details handle from here on.
						Session.JoinHandle.BackendHandle = MakeShared<FEOSSessionJoinHandle>(
							Details, LobbyId, Session.SessionName);

						Result.Sessions.Add(Session);
						EOS_LobbyDetails_Info_Release(Info);
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Lobby search failed: %d"), (int32)Data->ResultCode);
				}

				LocalCtx->Callback(Result);
				EOS_LobbySearch_Release(LocalCtx->SearchHandle);
			});
	}

	void FEOSMatchmaking::JoinLobbyById(const FString& LobbyId,
	                                    TFunction<void(const FSessionJoinResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: JoinLobbyById called when service not ready"));

		if (LobbyId.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: JoinLobbyById called with empty lobby id"));
			Callback(FSessionJoinResult(false));
			return;
		}

		if (bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Already in a lobby, leaving old lobby first"));
			LeaveSession([this, LobbyId, Callback](const FGamingServiceResult&)
			{
				JoinLobbyById(LobbyId, Callback);
			});
			return;
		}

		// Joined by id rather than through a search result, because the lobbies this creates are
		// EOS_LPL_INVITEONLY: deliberately absent from every search, including a search for their own id.
		// EOS_Lobby_JoinLobbyById is the only way in, and works for anyone holding the id — which is how a
		// Steam-delivered invite gets its recipient into an EOS lobby.
		const std::string LobbyIdUtf8 = TCHAR_TO_UTF8(*LobbyId);
		EOS_Lobby_JoinLobbyByIdOptions JoinOptions = {};
		JoinOptions.ApiVersion = EOS_LOBBY_JOINLOBBYBYID_API_LATEST;
		JoinOptions.LocalUserId = ProductUserId(Core);
		JoinOptions.LobbyId = LobbyIdUtf8.c_str();
		JoinOptions.bPresenceEnabled = EOS_FALSE;

		EOS_Lobby_JoinLobbyById(
			LobbyHandle(Core),
			&JoinOptions,
			FSessionJoinCallbackCtx::Create(this, MoveTemp(Callback)),
			[](const EOS_Lobby_JoinLobbyByIdCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionJoinCallbackCtx*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx->Service;
				check(Self);

				FSessionJoinResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
				if (!Result.bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: JoinLobbyById failed: %d"), (int32)Data->ResultCode);
					FSessionJoinCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				const FString JoinedLobbyId = Data->LobbyId ? UTF8_TO_TCHAR(Data->LobbyId) : FString();

				// Details are only reachable once we are a member, so they are copied here rather than
				// coming from a search result.
				const std::string JoinedIdUtf8 = TCHAR_TO_UTF8(*JoinedLobbyId);
				EOS_Lobby_CopyLobbyDetailsHandleOptions CopyOptions = {};
				CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
				CopyOptions.LobbyId = JoinedIdUtf8.c_str();
				CopyOptions.LocalUserId = ProductUserId(Self->Core);

				EOS_HLobbyDetails Details = nullptr;
				if (EOS_Lobby_CopyLobbyDetailsHandle(LobbyHandle(Self->Core), &CopyOptions, &Details) !=
					EOS_EResult::EOS_Success || !Details)
				{
					UE_LOG(LogTemp, Error,
					       TEXT("EOSGamingService: Joined lobby %s but could not read its details"), *JoinedLobbyId);
					Result.bSuccess = false;
					FSessionJoinCallbackCtx::Complete(LocalCtx, Result);
					return;
				}

				Self->FinalizeJoinedLobby(Details, JoinedLobbyId, FString(), Result);
				EOS_LobbyDetails_Release(Details);

				FSessionJoinCallbackCtx::Complete(LocalCtx, Result);
			});
	}

	void FEOSMatchmaking::JoinSession(const FSessionJoinHandle& JoinHandle,
	                                  TFunction<void(const FSessionJoinResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: JoinSession called when service not ready"));

		if (!JoinHandle.BackendHandle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: JoinSession requires a lobby from FindSessions or an invite."));
			Callback(FSessionJoinResult(false));
			return;
		}

		// A handle carrying only an id (a shared join code, or an invite delivered through another
		// platform) has no lobby details to join through — resolve it by id instead. Checked before the
		// cast below, which is only valid for this backend's own handle type.
		if (!JoinHandle.BackendHandle->HasBackendDetails())
		{
			const FString LobbyIdOnly = JoinHandle.BackendHandle->GetLobbyId();
			if (LobbyIdOnly.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("EOSGamingService: JoinSession got a handle with neither lobby details nor an id."));
				Callback(FSessionJoinResult(false));
				return;
			}
			UE_LOG(LogTemp, Log, TEXT("EOSGamingService: JoinSession resolving id-only handle '%s'"), *LobbyIdOnly);
			JoinLobbyById(LobbyIdOnly, MoveTemp(Callback));
			return;
		}

		const TSharedPtr<FEOSSessionJoinHandle> Handle =
			StaticCastSharedPtr<FEOSSessionJoinHandle>(JoinHandle.BackendHandle);

		if (bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Already in a lobby, leaving old lobby first"));
			LeaveSession([this, JoinHandle, Callback](const FGamingServiceResult&)
			{
				JoinSession(JoinHandle, Callback);
			});
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Joining lobby: %s ('%s')"), *Handle->LobbyId, *Handle->SessionName);

		struct FJoinLobbyCtx
		{
			FSessionJoinCallbackCtx* Ctx;
			TSharedPtr<FEOSSessionJoinHandle> JoinHandle;
		};
		auto* JoinCtx = new FJoinLobbyCtx{FSessionJoinCallbackCtx::Create(this, MoveTemp(Callback)), Handle};

		EOS_Lobby_JoinLobbyOptions JoinOptions = {};
		JoinOptions.ApiVersion = EOS_LOBBY_JOINLOBBY_API_LATEST;
		JoinOptions.LobbyDetailsHandle = Handle->Handle;
		JoinOptions.LocalUserId = ProductUserId(Core);
		JoinOptions.bPresenceEnabled = EOS_FALSE;

		EOS_Lobby_JoinLobby(
			LobbyHandle(Core),
			&JoinOptions,
			JoinCtx,
			[](const EOS_Lobby_JoinLobbyCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FJoinLobbyCtx> LocalCtx(static_cast<FJoinLobbyCtx*>(Data->ClientData));
				FEOSMatchmaking* Self = LocalCtx->Ctx->Service;
				check(Self);

				FSessionJoinResult Result;
				Result.bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

				if (!Result.bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to join lobby: %d"), (int32)Data->ResultCode);
					FSessionJoinCallbackCtx::Complete(LocalCtx->Ctx, Result);
					return;
				}

				Self->FinalizeJoinedLobby(
					LocalCtx->JoinHandle->Handle,
					Data->LobbyId ? UTF8_TO_TCHAR(Data->LobbyId) : LocalCtx->JoinHandle->LobbyId,
					LocalCtx->JoinHandle->SessionName,
					Result);

				FSessionJoinCallbackCtx::Complete(LocalCtx->Ctx, Result);
			});
	}

	void FEOSMatchmaking::FinalizeJoinedLobby(void* LobbyDetails, const FString& LobbyId,
	                                          const FString& SessionName, FSessionJoinResult& OutResult)
	{
		const EOS_HLobbyDetails Details = static_cast<EOS_HLobbyDetails>(LobbyDetails);

		EOS_LobbyDetails_GetLobbyOwnerOptions OwnerOptions = {};
		OwnerOptions.ApiVersion = EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
		const EOS_ProductUserId Owner = EOS_LobbyDetails_GetLobbyOwner(Details, &OwnerOptions);

		bIsInLobby = true;
		bIsLobbyOwner = false;
		CurrentLobbyId = LobbyId;
		CurrentLobbyOwnerPuid = PuidToString(Owner);
		CurrentSessionName = SessionName.IsEmpty()
			                     ? GetLobbyAttribute(Details, GSessionNameAttributeKey)
			                     : SessionName;

		OutResult.SessionInfo.SessionName = CurrentSessionName;
		OutResult.SessionInfo.HostUserId = CurrentLobbyOwnerPuid;
		OutResult.SessionInfo.HostDisplayName = GetMemberAttribute(Details, Owner, GDisplayNameAttributeKey);

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Joined lobby %s"), *CurrentLobbyId);

		// Advertise this member's display name so the host's join notification carries a real name.
		const std::string LobbyIdUtf8 = TCHAR_TO_UTF8(*CurrentLobbyId);
		EOS_Lobby_UpdateLobbyModificationOptions ModOptions = {};
		ModOptions.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
		ModOptions.LocalUserId = ProductUserId(Core);
		ModOptions.LobbyId = LobbyIdUtf8.c_str();

		EOS_HLobbyModification Modification = nullptr;
		if (EOS_Lobby_UpdateLobbyModification(LobbyHandle(Core), &ModOptions, &Modification) !=
			EOS_EResult::EOS_Success)
		{
			return;
		}

		const FTCHARToUTF8 DisplayNameUtf8(*Core.GetDisplayName());
		EOS_Lobby_AttributeData NameData = {};
		NameData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
		NameData.Key = GDisplayNameAttributeKey;
		NameData.ValueType = EOS_EAttributeType::EOS_AT_STRING;
		NameData.Value.AsUtf8 = DisplayNameUtf8.Get();

		EOS_LobbyModification_AddMemberAttributeOptions MemberAttrOptions = {};
		MemberAttrOptions.ApiVersion = EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST;
		MemberAttrOptions.Attribute = &NameData;
		MemberAttrOptions.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
		EOS_LobbyModification_AddMemberAttribute(Modification, &MemberAttrOptions);

		EOS_Lobby_UpdateLobbyOptions UpdateOptions = {};
		UpdateOptions.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
		UpdateOptions.LobbyModificationHandle = Modification;
		EOS_Lobby_UpdateLobby(
			LobbyHandle(Core), &UpdateOptions, Modification,
			[](const EOS_Lobby_UpdateLobbyCallbackInfo* UpdateData)
			{
				if (UpdateData->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("EOSGamingService: Failed to advertise member display name: %d"),
					       (int32)UpdateData->ResultCode);
				}
				EOS_LobbyModification_Release(static_cast<EOS_HLobbyModification>(UpdateData->ClientData));
			});
	}

	void FEOSMatchmaking::LeaveSession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: LeaveSession called when service not ready"));

		if (!bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Not in a lobby"));
			Callback(FGamingServiceResult(true));
			return;
		}

		if (bIsLobbyOwner)
		{
			DestroySession(MoveTemp(Callback));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Leaving lobby %s"), *CurrentLobbyId);

		auto* Ctx = FSessionUpdateCallbackCtx::Create(this, MoveTemp(Callback));

		const std::string LobbyIdUtf8 = TCHAR_TO_UTF8(*CurrentLobbyId);
		EOS_Lobby_LeaveLobbyOptions LeaveOptions = {};
		LeaveOptions.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
		LeaveOptions.LobbyId = LobbyIdUtf8.c_str();
		LeaveOptions.LocalUserId = ProductUserId(Core);

		EOS_Lobby_LeaveLobby(
			LobbyHandle(Core),
			&LeaveOptions,
			Ctx,
			[](const EOS_Lobby_LeaveLobbyCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionUpdateCallbackCtx*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx->Service;
				check(Self);

				const FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);
				if (Result.bSuccess)
				{
					Self->ResetLobbyState();
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Successfully left lobby"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to leave lobby: %d"), (int32)Data->ResultCode);
				}
				FSessionUpdateCallbackCtx::Complete(LocalCtx, Result);
			});
	}

	void FEOSMatchmaking::DestroySession(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: DestroySession called when service not ready"));

		if (!bIsInLobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: Not in a lobby"));
			Callback(FGamingServiceResult(true));
			return;
		}

		if (!bIsLobbyOwner)
		{
			// Only the owner can destroy the lobby; for a member "destroy" degrades to leaving it.
			LeaveSession(MoveTemp(Callback));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Destroying lobby %s"), *CurrentLobbyId);

		auto* Ctx = FSessionUpdateCallbackCtx::Create(this, MoveTemp(Callback));

		const std::string LobbyIdUtf8 = TCHAR_TO_UTF8(*CurrentLobbyId);
		EOS_Lobby_DestroyLobbyOptions DestroyOptions = {};
		DestroyOptions.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
		DestroyOptions.LobbyId = LobbyIdUtf8.c_str();
		DestroyOptions.LocalUserId = ProductUserId(Core);

		EOS_Lobby_DestroyLobby(
			LobbyHandle(Core),
			&DestroyOptions,
			Ctx,
			[](const EOS_Lobby_DestroyLobbyCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				auto* LocalCtx = static_cast<FSessionUpdateCallbackCtx*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx->Service;
				check(Self);

				const FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);
				if (Result.bSuccess)
				{
					Self->ResetLobbyState();
					UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Lobby destroyed"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to destroy lobby: %d"), (int32)Data->ResultCode);
				}
				FSessionUpdateCallbackCtx::Complete(LocalCtx, Result);
			});
	}

	void FEOSMatchmaking::UpdateSession(const FSessionSettings& Settings,
	                                    TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: UpdateSession called when service not ready"));

		if (!bIsInLobby || !bIsLobbyOwner)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot update lobby - not the lobby owner"));
			Callback(FGamingServiceResult(false));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Updating lobby"));
		ApplySessionSettings(Settings, TEXT("update"), TEXT("EOSGamingService: Lobby updated successfully"), MoveTemp(Callback));
	}

	void FEOSMatchmaking::LockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: LockLobby called when service not ready"));

		if (!bIsInLobby || !bIsLobbyOwner)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot lock lobby - not the lobby owner"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Locking lobby"));

		FSessionSettings LockedSettings = CurrentSessionSettings;
		LockedSettings.Privacy = ESessionPrivacy::Private;
		LockedSettings.bAllowInvites = false;
		LockedSettings.bAllowJoinInProgress = false;

		ApplySessionSettings(LockedSettings, TEXT("lock"), TEXT("EOSGamingService: Lobby locked successfully"), MoveTemp(Callback));
	}

	void FEOSMatchmaking::UnlockLobby(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: UnlockLobby called when service not ready"));

		if (!bIsInLobby || !bIsLobbyOwner)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot unlock lobby - not the lobby owner"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("EOSGamingService: Unlocking lobby"));

		FSessionSettings UnlockedSettings = CurrentSessionSettings;
		UnlockedSettings.Privacy = ESessionPrivacy::Public;
		UnlockedSettings.bAllowInvites = true;
		UnlockedSettings.bAllowJoinInProgress = true;

		ApplySessionSettings(UnlockedSettings, TEXT("unlock"), TEXT("EOSGamingService: Lobby unlocked successfully"), MoveTemp(Callback));
	}

	void FEOSMatchmaking::ApplySessionSettings(const FSessionSettings& Settings, const TCHAR* OperationName,
	                                           const TCHAR* SuccessMessage,
	                                           TFunction<void(const FGamingServiceResult&)> Callback)
	{
		const std::string LobbyIdUtf8 = TCHAR_TO_UTF8(*CurrentLobbyId);

		EOS_Lobby_UpdateLobbyModificationOptions ModOptions = {};
		ModOptions.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
		ModOptions.LocalUserId = ProductUserId(Core);
		ModOptions.LobbyId = LobbyIdUtf8.c_str();

		EOS_HLobbyModification Modification = nullptr;
		const EOS_EResult ModResult = EOS_Lobby_UpdateLobbyModification(LobbyHandle(Core), &ModOptions, &Modification);
		if (ModResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to create lobby modification for %s: %d"),
			       OperationName, (int32)ModResult);
			Callback(FGamingServiceResult(false));
			return;
		}

		EOS_LobbyModification_SetMaxMembersOptions MaxOptions = {};
		MaxOptions.ApiVersion = EOS_LOBBYMODIFICATION_SETMAXMEMBERS_API_LATEST;
		MaxOptions.MaxMembers = Settings.MaxPlayers;
		EOS_LobbyModification_SetMaxMembers(Modification, &MaxOptions);

		// Privacy covers the whole lock/unlock surface for lobbies: EOS has no separate
		// join-in-progress toggle, so bAllowJoinInProgress maps into the permission level.
		EOS_LobbyModification_SetPermissionLevelOptions PermissionOptions = {};
		PermissionOptions.ApiVersion = EOS_LOBBYMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
		PermissionOptions.PermissionLevel = ToLobbyPermission(Settings.Privacy);
		const EOS_EResult PermissionResult = EOS_LobbyModification_SetPermissionLevel(Modification, &PermissionOptions);

		EOS_LobbyModification_SetInvitesAllowedOptions InvitesOptions = {};
		InvitesOptions.ApiVersion = EOS_LOBBYMODIFICATION_SETINVITESALLOWED_API_LATEST;
		InvitesOptions.bInvitesAllowed = Settings.bAllowInvites ? EOS_TRUE : EOS_FALSE;
		const EOS_EResult InvitesResult = EOS_LobbyModification_SetInvitesAllowed(Modification, &InvitesOptions);

		if (PermissionResult != EOS_EResult::EOS_Success || InvitesResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to configure lobby %s (permission=%d, invites=%d)"),
			       OperationName, (int32)PermissionResult, (int32)InvitesResult);
			EOS_LobbyModification_Release(Modification);
			Callback(FGamingServiceResult(false));
			return;
		}

		// Advertise the human-facing session name plus every custom attribute.
		{
			const FTCHARToUTF8 SessionNameUtf8(*Settings.SessionName);
			EOS_Lobby_AttributeData NameData = {};
			NameData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
			NameData.Key = GSessionNameAttributeKey;
			NameData.ValueType = EOS_EAttributeType::EOS_AT_STRING;
			NameData.Value.AsUtf8 = SessionNameUtf8.Get();

			EOS_LobbyModification_AddAttributeOptions AttrOptions = {};
			AttrOptions.ApiVersion = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
			AttrOptions.Attribute = &NameData;
			AttrOptions.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
			EOS_LobbyModification_AddAttribute(Modification, &AttrOptions);
		}

		for (const FSessionAttribute& Attr : Settings.CustomAttributes)
		{
			const FTCHARToUTF8 KeyUtf8(*Attr.Key);
			const FTCHARToUTF8 ValueUtf8(*Attr.Value);

			EOS_Lobby_AttributeData AttributeData = {};
			AttributeData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
			AttributeData.Key = KeyUtf8.Get();
			AttributeData.ValueType = EOS_EAttributeType::EOS_AT_STRING;
			AttributeData.Value.AsUtf8 = ValueUtf8.Get();

			EOS_LobbyModification_AddAttributeOptions AttrOptions = {};
			AttrOptions.ApiVersion = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
			AttrOptions.Attribute = &AttributeData;
			AttrOptions.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
			EOS_LobbyModification_AddAttribute(Modification, &AttrOptions);
		}

		// The owner also advertises their display name as a member attribute.
		{
			const FTCHARToUTF8 DisplayNameUtf8(*Core.GetDisplayName());
			EOS_Lobby_AttributeData NameData = {};
			NameData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
			NameData.Key = GDisplayNameAttributeKey;
			NameData.ValueType = EOS_EAttributeType::EOS_AT_STRING;
			NameData.Value.AsUtf8 = DisplayNameUtf8.Get();

			EOS_LobbyModification_AddMemberAttributeOptions MemberAttrOptions = {};
			MemberAttrOptions.ApiVersion = EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST;
			MemberAttrOptions.Attribute = &NameData;
			MemberAttrOptions.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
			EOS_LobbyModification_AddMemberAttribute(Modification, &MemberAttrOptions);
		}

		struct FUpdateLobbyContext
		{
			FSessionUpdateCallbackCtx* CallbackCtx;
			EOS_HLobbyModification Modification;
			FSessionSettings UpdatedSettings;
			FString OperationName;
			FString SuccessMessage;
		};
		auto* UpdateCtx = new FUpdateLobbyContext{
			FSessionUpdateCallbackCtx::Create(this, MoveTemp(Callback)),
			Modification, Settings, OperationName, SuccessMessage};

		EOS_Lobby_UpdateLobbyOptions UpdateOptions = {};
		UpdateOptions.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
		UpdateOptions.LobbyModificationHandle = Modification;

		EOS_Lobby_UpdateLobby(
			LobbyHandle(Core),
			&UpdateOptions,
			UpdateCtx,
			[](const EOS_Lobby_UpdateLobbyCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FUpdateLobbyContext> LocalCtx(static_cast<FUpdateLobbyContext*>(Data->ClientData));
				FEOSMatchmaking* Self = LocalCtx->CallbackCtx->Service;
				check(Self);

				const FGamingServiceResult Result(Data->ResultCode == EOS_EResult::EOS_Success);
				if (Result.bSuccess)
				{
					Self->CurrentSessionSettings = LocalCtx->UpdatedSettings;
					Self->CurrentSessionName = LocalCtx->UpdatedSettings.SessionName;
					UE_LOG(LogTemp, Log, TEXT("%s"), *LocalCtx->SuccessMessage);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Failed to %s lobby: %d"),
					       *LocalCtx->OperationName, (int32)Data->ResultCode);
				}

				EOS_LobbyModification_Release(LocalCtx->Modification);
				FSessionUpdateCallbackCtx::Complete(LocalCtx->CallbackCtx, Result);
			});
	}

	void FEOSMatchmaking::GetCurrentSession(TFunction<void(const FSessionInfo&)> Callback)
	{
		FSessionInfo Info;

		if (bIsInLobby)
		{
			Info.SessionName = CurrentSessionName;
			Info.HostUserId = CurrentLobbyOwnerPuid;
			Info.MaxPlayers = CurrentSessionSettings.MaxPlayers;
			Info.Privacy = CurrentSessionSettings.Privacy;
			Info.bAllowJoinInProgress = CurrentSessionSettings.bAllowJoinInProgress;
			Info.CustomAttributes = CurrentSessionSettings.CustomAttributes;
			Info.HostDisplayName = bIsLobbyOwner ? Core.GetDisplayName() : FString();

			// Live membership numbers come from the local lobby cache.
			if (EOS_HLobbyDetails Details = CopyCurrentLobbyDetails(Core, CurrentLobbyId))
			{
				EOS_LobbyDetails_CopyInfoOptions InfoOptions = {};
				InfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;

				EOS_LobbyDetails_Info* LobbyInfo = nullptr;
				if (EOS_LobbyDetails_CopyInfo(Details, &InfoOptions, &LobbyInfo) == EOS_EResult::EOS_Success && LobbyInfo)
				{
					Info.MaxPlayers = (int32)LobbyInfo->MaxMembers;
					Info.AvailableSlots = (int32)LobbyInfo->AvailableSlots;
					Info.CurrentPlayers = Info.MaxPlayers - Info.AvailableSlots;
					EOS_LobbyDetails_Info_Release(LobbyInfo);
				}
				if (!bIsLobbyOwner)
				{
					EOS_LobbyDetails_GetLobbyOwnerOptions OwnerOptions = {};
					OwnerOptions.ApiVersion = EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
					const EOS_ProductUserId Owner = EOS_LobbyDetails_GetLobbyOwner(Details, &OwnerOptions);
					Info.HostDisplayName = GetMemberAttribute(Details, Owner, GDisplayNameAttributeKey);
				}
				EOS_LobbyDetails_Release(Details);
			}
		}

		Callback(Info);
	}

	void FEOSMatchmaking::ShowInviteFriendsDialog(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		checkf(Core.IsInitialized() && Core.IsLoggedIn() && Core.GetLobbyHandle(),
		       TEXT("EOSGamingService: ShowInviteFriendsDialog called when service not ready"));

		if (!bIsInLobby || CurrentLobbyId.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: Cannot show invite dialog - not in a lobby"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		// EOS has no friend-picker overlay of its own. Report failure rather than success: claiming to have
		// shown a dialog that does not exist leaves the caller believing the player is picking friends, so
		// its button looks dead instead of falling through to the in-game picker. Callers branch on
		// PlatformOwnsInviteUI() to choose up front; this is the backstop for the ones that just call.
		UE_LOG(LogTemp, Log,
		       TEXT("EOSGamingService: no built-in invite dialog on this backend - the game must show its own"));

		if (Callback)
		{
			Callback(FGamingServiceResult(false));
		}
	}

	bool FEOSMatchmaking::PlatformOwnsInviteUI() const
	{
		// The EOS Social Overlay presents incoming invites itself, and it exists on desktop only - there is
		// no overlay on Android or iOS, where an invite that the game does not draw is an invite the player
		// never sees. So this is a platform split, not a backend-wide answer.
		//
		// Caveat worth knowing if desktop invites ever stop appearing: the overlay is only present when the
		// title runs with it loaded (launched through the Epic launcher or the EOS bootstrapper). A build
		// started without it would report true here and then show nothing, in which case this wants to
		// become a runtime check rather than a compile-time one.
#if PLATFORM_DESKTOP
		return true;
#else
		return false;
#endif
	}

	void FEOSMatchmaking::QueryPendingInvites(TFunction<void(const FPendingInvitesResult&)> Callback)
	{
		check(Callback);

		if (!LobbyHandle(Core) || !ProductUserId(Core))
		{
			UE_LOG(LogTemp, Warning, TEXT("EOSGamingService: cannot query invites - not signed in"));
			Callback(FPendingInvitesResult(false));
			return;
		}

		EOS_Lobby_QueryInvitesOptions Options = {};
		Options.ApiVersion = EOS_LOBBY_QUERYINVITES_API_LATEST;
		Options.LocalUserId = ProductUserId(Core);

		struct FQueryInvitesCtx
		{
			FEOSMatchmaking* Service = nullptr;
			TFunction<void(const FPendingInvitesResult&)> Callback;
		};
		auto* Ctx = new FQueryInvitesCtx{this, MoveTemp(Callback)};

		EOS_Lobby_QueryInvites(
			LobbyHandle(Core),
			&Options,
			Ctx,
			[](const EOS_Lobby_QueryInvitesCallbackInfo* Data)
			{
				check(Data);
				auto* LocalCtx = static_cast<FQueryInvitesCtx*>(Data->ClientData);
				FEOSMatchmaking* Self = LocalCtx->Service;

				FPendingInvitesResult Result;

				// The query only refreshes the local cache; the invites themselves are read back out of it
				// index by index below. EOS_NotFound simply means the cache is empty, which is the ordinary
				// case and not a failure worth reporting as one.
				const bool bQueryOk = Data->ResultCode == EOS_EResult::EOS_Success ||
					Data->ResultCode == EOS_EResult::EOS_NotFound;

				if (bQueryOk && Self && LobbyHandle(Self->Core) && ProductUserId(Self->Core))
				{
					Result.bSuccess = true;

					EOS_Lobby_GetInviteCountOptions CountOptions = {};
					CountOptions.ApiVersion = EOS_LOBBY_GETINVITECOUNT_API_LATEST;
					CountOptions.LocalUserId = ProductUserId(Self->Core);

					const uint32 Count = EOS_Lobby_GetInviteCount(LobbyHandle(Self->Core), &CountOptions);
					for (uint32 Index = 0; Index < Count; ++Index)
					{
						EOS_Lobby_GetInviteIdByIndexOptions IdOptions = {};
						IdOptions.ApiVersion = EOS_LOBBY_GETINVITEIDBYINDEX_API_LATEST;
						IdOptions.LocalUserId = ProductUserId(Self->Core);
						IdOptions.Index = Index;

						char IdBuffer[EOS_LOBBY_INVITEID_MAX_LENGTH + 1] = {};
						int32 IdLength = sizeof(IdBuffer);
						if (EOS_Lobby_GetInviteIdByIndex(LobbyHandle(Self->Core), &IdOptions, IdBuffer, &IdLength) !=
							EOS_EResult::EOS_Success)
						{
							continue;
						}

						// The sender is not exposed by the invite cache; it is read off the lobby details
						// inside BuildInviteInfo, so pass null and let the display name come from there.
						FLobbyInviteReceivedInfo Info;
						if (BuildInviteInfo(Self->Core, IdBuffer, nullptr, Info))
						{
							Result.Invites.Add(MoveTemp(Info));
						}
					}
				}

				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: pending invite query -> %d invite(s), result %d"),
				       Result.Invites.Num(), (int32)Data->ResultCode);

				if (LocalCtx->Callback)
				{
					LocalCtx->Callback(Result);
				}
				delete LocalCtx;
			});
	}

	void FEOSMatchmaking::RejectInvite(const FString& InviteId,
	                                   TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (InviteId.IsEmpty() || !LobbyHandle(Core) || !ProductUserId(Core))
		{
			UE_LOG(LogTemp, Error, TEXT("EOSGamingService: cannot reject invite - no invite id or not signed in"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		// Must outlive EOS_Lobby_RejectInvite below, which copies the string out of the options struct.
		const std::string InviteIdUtf8 = TCHAR_TO_UTF8(*InviteId);

		EOS_Lobby_RejectInviteOptions Options = {};
		Options.ApiVersion = EOS_LOBBY_REJECTINVITE_API_LATEST;
		Options.InviteId = InviteIdUtf8.c_str();
		Options.LocalUserId = ProductUserId(Core);

		auto* Ctx = new TFunction<void(const FGamingServiceResult&)>(MoveTemp(Callback));

		EOS_Lobby_RejectInvite(
			LobbyHandle(Core),
			&Options,
			Ctx,
			[](const EOS_Lobby_RejectInviteCallbackInfo* Data)
			{
				check(Data);
				auto* LocalCtx = static_cast<TFunction<void(const FGamingServiceResult&)>*>(Data->ClientData);

				const bool bSuccess = Data->ResultCode == EOS_EResult::EOS_Success;
				UE_LOG(LogTemp, Log, TEXT("EOSGamingService: reject invite %hs -> %d"),
				       Data->InviteId ? Data->InviteId : "null", (int32)Data->ResultCode);

				if (LocalCtx && *LocalCtx)
				{
					(*LocalCtx)(FGamingServiceResult(bSuccess));
				}
				delete LocalCtx;
			});
	}

	FString FEOSMatchmaking::GetSessionConnectionString() const
	{
		// The connection string names the lobby OWNER (the listen host the net driver must reach),
		// for members and host alike.
		if (!bIsInLobby || CurrentLobbyOwnerPuid.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(TEXT("eos.%s"), *CurrentLobbyOwnerPuid);
	}
}

#endif // GS_WITH_EOS

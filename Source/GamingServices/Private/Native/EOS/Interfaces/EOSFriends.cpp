#if defined(GS_WITH_EOS)

#include "Native/EOS/Interfaces/EOSFriends.h"
#include "EOSCommon.h"
#include "Native/EOS/EOSPlatformCore.h"
#include "Native/Interfaces/IMatchmakingService.h"

namespace GamingServices
{
	namespace
	{
		/** EOS presence collapsed onto the states every backend can report. */
		EGamingFriendState ToFriendState(EOS_Presence_EStatus Status)
		{
			switch (Status)
			{
			case EOS_Presence_EStatus::EOS_PS_Online:
				return EGamingFriendState::Online;
			case EOS_Presence_EStatus::EOS_PS_Away:
			case EOS_Presence_EStatus::EOS_PS_ExtendedAway:
				return EGamingFriendState::Away;
			case EOS_Presence_EStatus::EOS_PS_DoNotDisturb:
				return EGamingFriendState::Busy;
			case EOS_Presence_EStatus::EOS_PS_Offline:
			default:
				return EGamingFriendState::Offline;
			}
		}

		FString EpicIdToString(EOS_EpicAccountId Id)
		{
			if (!Id || EOS_EpicAccountId_IsValid(Id) == EOS_FALSE)
			{
				return FString();
			}

			char Buffer[EOS_EPICACCOUNTID_MAX_LENGTH + 1] = {};
			int32_t Length = sizeof(Buffer);
			if (EOS_EpicAccountId_ToString(Id, Buffer, &Length) != EOS_EResult::EOS_Success)
			{
				return FString();
			}
			return UTF8_TO_TCHAR(Buffer);
		}
	}

	struct FEOSFriends::FImpl
	{
		FEOSFriends& Owner;

		TArray<FGamingFriend> Cached;

		/** Set while a QueryFriends is in flight, so overlapping calls do not interleave into the cache. */
		bool bQueryInFlight = false;

		EOS_NotificationId FriendsUpdateNotification = EOS_INVALID_NOTIFICATIONID;

		explicit FImpl(FEOSFriends& InOwner)
			: Owner(InOwner)
		{
		}

		EOS_HFriends FriendsHandle() const
		{
			return static_cast<EOS_HFriends>(Owner.Core.GetFriendsHandle());
		}

		EOS_HPresence PresenceHandle() const
		{
			return static_cast<EOS_HPresence>(Owner.Core.GetPresenceHandle());
		}

		EOS_HUserInfo UserInfoHandle() const
		{
			return static_cast<EOS_HUserInfo>(Owner.Core.GetUserInfoHandle());
		}

		EOS_HConnect ConnectHandle() const
		{
			return static_cast<EOS_HConnect>(Owner.Core.GetConnectHandle());
		}

		EOS_HLobby LobbyHandle() const
		{
			return static_cast<EOS_HLobby>(Owner.Core.GetLobbyHandle());
		}

		EOS_EpicAccountId LocalEpicId() const
		{
			return static_cast<EOS_EpicAccountId>(Owner.Core.GetEpicAccountId());
		}

		EOS_ProductUserId LocalProductUserId() const
		{
			return static_cast<EOS_ProductUserId>(Owner.Core.GetProductUserId());
		}

		/** Subscribe once, lazily, so a service that never reads friends costs nothing. */
		void EnsureFriendsUpdateSubscription()
		{
			if (FriendsUpdateNotification != EOS_INVALID_NOTIFICATIONID || !FriendsHandle())
			{
				return;
			}

			EOS_Friends_AddNotifyFriendsUpdateOptions Options = {};
			Options.ApiVersion = EOS_FRIENDS_ADDNOTIFYFRIENDSUPDATE_API_LATEST;

			FriendsUpdateNotification = EOS_Friends_AddNotifyFriendsUpdate(
				FriendsHandle(),
				&Options,
				this,
				[](const EOS_Friends_OnFriendsUpdateInfo* Data)
				{
					check(Data);
					check(Data->ClientData);
					auto* Self = static_cast<FImpl*>(Data->ClientData);
					if (Self->Owner.OnFriendsChanged)
					{
						Self->Owner.OnFriendsChanged();
					}
				});
		}

		void ReleaseFriendsUpdateSubscription()
		{
			if (FriendsUpdateNotification == EOS_INVALID_NOTIFICATIONID)
			{
				return;
			}
			if (FriendsHandle())
			{
				EOS_Friends_RemoveNotifyFriendsUpdate(FriendsHandle(), FriendsUpdateNotification);
			}
			FriendsUpdateNotification = EOS_INVALID_NOTIFICATIONID;
		}

		/** Read the ids EOS cached for the completed QueryFriends, keeping only accepted friendships. */
		TArray<EOS_EpicAccountId> CollectAcceptedFriends() const
		{
			TArray<EOS_EpicAccountId> Ids;

			EOS_Friends_GetFriendsCountOptions CountOptions = {};
			CountOptions.ApiVersion = EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST;
			CountOptions.LocalUserId = LocalEpicId();

			const int32 Count = EOS_Friends_GetFriendsCount(FriendsHandle(), &CountOptions);
			Ids.Reserve(FMath::Max(0, Count));

			// An empty friend list and a list where everything was filtered out look identical from the
			// outside, and they have completely different causes, so say which happened.
			int32 NumSkippedByStatus = 0;

			for (int32 Index = 0; Index < Count; ++Index)
			{
				EOS_Friends_GetFriendAtIndexOptions AtOptions = {};
				AtOptions.ApiVersion = EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST;
				AtOptions.LocalUserId = LocalEpicId();
				AtOptions.Index = Index;

				EOS_EpicAccountId FriendId = EOS_Friends_GetFriendAtIndex(FriendsHandle(), &AtOptions);
				if (!FriendId || EOS_EpicAccountId_IsValid(FriendId) == EOS_FALSE)
				{
					continue;
				}

				// The list also carries pending invitations in both directions; those are not friends yet
				// and cannot be invited to anything, so they do not belong in a friend list.
				EOS_Friends_GetStatusOptions StatusOptions = {};
				StatusOptions.ApiVersion = EOS_FRIENDS_GETSTATUS_API_LATEST;
				StatusOptions.LocalUserId = LocalEpicId();
				StatusOptions.TargetUserId = FriendId;

				const EOS_EFriendsStatus Status = EOS_Friends_GetStatus(FriendsHandle(), &StatusOptions);
				if (Status != EOS_EFriendsStatus::EOS_FS_Friends)
				{
					++NumSkippedByStatus;
					UE_LOG(LogTemp, Verbose, TEXT("EOSFriends: skipping '%s' - friendship status %d, not an accepted friend"),
					       *EpicIdToString(FriendId), (int32)Status);
					continue;
				}

				Ids.Add(FriendId);
			}

			UE_LOG(LogTemp, Log,
			       TEXT("EOSFriends: Epic friends list returned %d entr%s; %d accepted, %d skipped as not-yet-friends. "
			            "EOS_AS_FriendsList is scoped to \"friends who use this application\" - Epic filters out any "
			            "friend who has never signed into this product, so an empty list is normal and does NOT mean "
			            "the user has no Epic friends. Not Steam friends, not lobby members."),
			       Count, Count == 1 ? TEXT("y") : TEXT("ies"), Ids.Num(), NumSkippedByStatus);

			return Ids;
		}

		/** Read whatever UserInfo has cached for this friend; empty when the name has not arrived. */
		FString ReadCachedDisplayName(EOS_EpicAccountId FriendId) const
		{
			const auto ExtractName = [](EOS_UserInfo_BestDisplayName* Best) -> FString
			{
				if (!Best)
				{
					return FString();
				}
				const char* Name = Best->DisplayName ? Best->DisplayName : Best->DisplayNameSanitized;
				FString Result = Name ? UTF8_TO_TCHAR(Name) : FString();
				EOS_UserInfo_BestDisplayName_Release(Best);
				return Result;
			};

			EOS_UserInfo_CopyBestDisplayNameOptions Options = {};
			Options.ApiVersion = EOS_USERINFO_COPYBESTDISPLAYNAME_API_LATEST;
			Options.LocalUserId = LocalEpicId();
			Options.TargetUserId = FriendId;

			EOS_UserInfo_BestDisplayName* Best = nullptr;
			if (EOS_UserInfo_CopyBestDisplayName(UserInfoHandle(), &Options, &Best) == EOS_EResult::EOS_Success)
			{
				return ExtractName(Best);
			}

			// "Best" means best ACROSS LINKED PLATFORMS, and that resolution is keyed on the target's
			// ProductUserId, not their EpicAccountId. A friend with no linked external account fails
			// with EOS_NotFound ("unable to find Product User ID for target") even though QueryUserInfo
			// already cached their Epic name. Asking for the Epic platform explicitly reads exactly
			// that. Same fallback FEOSPlatformCore uses for the local user.
			EOS_UserInfo_CopyBestDisplayNameWithPlatformOptions PlatformOptions = {};
			PlatformOptions.ApiVersion = EOS_USERINFO_COPYBESTDISPLAYNAMEWITHPLATFORM_API_LATEST;
			PlatformOptions.LocalUserId = LocalEpicId();
			PlatformOptions.TargetUserId = FriendId;
			PlatformOptions.TargetPlatformType = EOS_OPT_Epic;

			EOS_UserInfo_BestDisplayName* PlatformBest = nullptr;
			if (EOS_UserInfo_CopyBestDisplayNameWithPlatform(
				UserInfoHandle(), &PlatformOptions, &PlatformBest) == EOS_EResult::EOS_Success)
			{
				return ExtractName(PlatformBest);
			}

			return FString();
		}

		/** Read whatever Presence has cached for this friend; Offline when nothing has arrived. */
		EGamingFriendState ReadCachedPresence(EOS_EpicAccountId FriendId) const
		{
			EOS_Presence_CopyPresenceOptions Options = {};
			Options.ApiVersion = EOS_PRESENCE_COPYPRESENCE_API_LATEST;
			Options.LocalUserId = LocalEpicId();
			Options.TargetUserId = FriendId;

			EOS_Presence_Info* Info = nullptr;
			if (EOS_Presence_CopyPresence(PresenceHandle(), &Options, &Info) != EOS_EResult::EOS_Success
				|| !Info)
			{
				return EGamingFriendState::Offline;
			}

			const EGamingFriendState State = ToFriendState(Info->Status);
			EOS_Presence_Info_Release(Info);
			return State;
		}

		/**
		 * Assemble the final list from whatever the per-friend queries managed to cache, then finish.
		 * Deliberately tolerant: a friend whose name or presence never arrived still appears, with the id
		 * as the name and Offline as the state, rather than vanishing from the list.
		 */
		void FinishQuery(const TArray<EOS_EpicAccountId>& Ids,
		                 TFunction<void(const FQueryFriendsResult&)> Callback)
		{
			TArray<FGamingFriend> Friends;
			Friends.Reserve(Ids.Num());

			for (EOS_EpicAccountId Id : Ids)
			{
				FGamingFriend Entry;
				Entry.UserId = EpicIdToString(Id);
				if (Entry.UserId.IsEmpty())
				{
					continue;
				}

				const FString Name = ReadCachedDisplayName(Id);
				Entry.DisplayName = Name.IsEmpty() ? Entry.UserId : Name;
				Entry.State = ReadCachedPresence(Id);

				// NOT DETERMINED on EOS. Telling "online somewhere" from "online in this game" means
				// comparing EOS_Presence_Info::ProductId against our own, and FEOSPlatformCore does not
				// expose the configured ProductId today. Left false rather than guessed: Steam fills this
				// in properly, and on EOS a false here only costs list ordering, never correctness.
				Entry.bPlayingThisGame = false;

				Friends.Add(MoveTemp(Entry));
			}

			Friends.StableSort([](const FGamingFriend& A, const FGamingFriend& B)
			{
				if (A.IsOnline() != B.IsOnline())
				{
					return A.IsOnline();
				}
				return A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase) < 0;
			});

			Cached = Friends;
			bQueryInFlight = false;

			UE_LOG(LogTemp, Log, TEXT("EOSFriends: resolved %d friends"), Cached.Num());
			Callback(FQueryFriendsResult::Succeeded(MoveTemp(Friends)));
		}
	};

	/**
	 * Shared state for the per-friend name/presence fan-out. Each outstanding query holds a reference; the
	 * last one to complete runs Finish. Refcounted rather than owned by FImpl so a query still completes
	 * safely if the service is torn down mid-flight.
	 */
	namespace
	{
		struct FFanOut
		{
			int32 Outstanding = 0;
			TFunction<void()> Finish;

			void Begin(int32 Count)
			{
				Outstanding = Count;
			}

			void One()
			{
				if (--Outstanding <= 0 && Finish)
				{
					Finish();
					Finish = nullptr;
				}
			}
		};
	}

	FEOSFriends::FEOSFriends(FEOSPlatformCore& InCore, IMatchmakingService& InMatchmaking)
		: Core(InCore)
		, Matchmaking(InMatchmaking)
		, Impl(MakePimpl<FImpl>(*this))
	{
	}

	FEOSFriends::~FEOSFriends()
	{
		Impl->ReleaseFriendsUpdateSubscription();
	}

	bool FEOSFriends::IsAvailable() const
	{
		// The EpicAccountId is the real gate: the handles exist for every platform, but a Connect-only
		// user has no Epic social graph, so Friends and Presence have nothing to answer with.
		return Core.IsInitialized()
			&& Impl->FriendsHandle() != nullptr
			&& Impl->PresenceHandle() != nullptr
			&& Impl->LocalEpicId() != nullptr
			&& EOS_EpicAccountId_IsValid(Impl->LocalEpicId()) == EOS_TRUE;
	}

	void FEOSFriends::QueryFriends(TFunction<void(const FQueryFriendsResult&)> Callback)
	{
		check(Callback);

		if (!IsAvailable())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("EOSFriends: no Epic friends available for this user (needs an EpicAccountId from "
			            "EOS_Auth_Login; a Connect-only sign-in, e.g. Steam auth into EOS, has none)"));
			Callback(FQueryFriendsResult::Failed());
			return;
		}

		if (Impl->bQueryInFlight)
		{
			// Serving the cache is better than racing two fan-outs into it.
			UE_LOG(LogTemp, Verbose, TEXT("EOSFriends: query already in flight; serving cache"));
			TArray<FGamingFriend> Copy = Impl->Cached;
			Callback(FQueryFriendsResult::Succeeded(MoveTemp(Copy)));
			return;
		}

		Impl->bQueryInFlight = true;
		Impl->EnsureFriendsUpdateSubscription();

		struct FQueryCtx
		{
			FEOSFriends::FImpl* Impl;
			TFunction<void(const FQueryFriendsResult&)> Callback;
		};
		auto* Ctx = new FQueryCtx{Impl.Get(), MoveTemp(Callback)};

		EOS_Friends_QueryFriendsOptions Options = {};
		Options.ApiVersion = EOS_FRIENDS_QUERYFRIENDS_API_LATEST;
		Options.LocalUserId = Impl->LocalEpicId();

		EOS_Friends_QueryFriends(
			Impl->FriendsHandle(),
			&Options,
			Ctx,
			[](const EOS_Friends_QueryFriendsCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FQueryCtx> LocalCtx(static_cast<FQueryCtx*>(Data->ClientData));
				FEOSFriends::FImpl* Self = LocalCtx->Impl;

				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSFriends: QueryFriends failed: %s"),
					       *FString(EOS_EResult_ToString(Data->ResultCode)));
					Self->bQueryInFlight = false;
					LocalCtx->Callback(FQueryFriendsResult::Failed());
					return;
				}

				const TArray<EOS_EpicAccountId> Ids = Self->CollectAcceptedFriends();
				if (Ids.IsEmpty())
				{
					Self->FinishQuery(Ids, MoveTemp(LocalCtx->Callback));
					return;
				}

				// Names and presence are separate interfaces and separate round-trips. Fire both per
				// friend and assemble once the last reply lands.
				auto FanOut = MakeShared<FFanOut>();
				TArray<EOS_EpicAccountId> IdsCopy = Ids;
				TFunction<void(const FQueryFriendsResult&)> Done = MoveTemp(LocalCtx->Callback);
				FanOut->Finish = [Self, IdsCopy, Done]() mutable
				{
					Self->FinishQuery(IdsCopy, MoveTemp(Done));
				};
				FanOut->Begin(Ids.Num() * 2);

				struct FPerFriendCtx
				{
					TSharedPtr<FFanOut> FanOut;
				};

				for (EOS_EpicAccountId FriendId : Ids)
				{
					EOS_UserInfo_QueryUserInfoOptions NameOptions = {};
					NameOptions.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
					NameOptions.LocalUserId = Self->LocalEpicId();
					NameOptions.TargetUserId = FriendId;

					EOS_UserInfo_QueryUserInfo(
						Self->UserInfoHandle(),
						&NameOptions,
						new FPerFriendCtx{FanOut},
						[](const EOS_UserInfo_QueryUserInfoCallbackInfo* NameData)
						{
							check(NameData);
							const TUniquePtr<FPerFriendCtx> C(static_cast<FPerFriendCtx*>(NameData->ClientData));
							C->FanOut->One();
						});

					EOS_Presence_QueryPresenceOptions PresenceOptions = {};
					PresenceOptions.ApiVersion = EOS_PRESENCE_QUERYPRESENCE_API_LATEST;
					PresenceOptions.LocalUserId = Self->LocalEpicId();
					PresenceOptions.TargetUserId = FriendId;

					EOS_Presence_QueryPresence(
						Self->PresenceHandle(),
						&PresenceOptions,
						new FPerFriendCtx{FanOut},
						[](const EOS_Presence_QueryPresenceCallbackInfo* PresenceData)
						{
							check(PresenceData);
							const TUniquePtr<FPerFriendCtx> C(
								static_cast<FPerFriendCtx*>(PresenceData->ClientData));
							C->FanOut->One();
						});
				}
			});
	}

	const TArray<FGamingFriend>& FEOSFriends::GetCachedFriends() const
	{
		return Impl->Cached;
	}

	void FEOSFriends::SendInvite(const FString& FriendUserId,
	                             TFunction<void(const FGamingServiceResult&)> Callback)
	{
		check(Callback);

		if (!IsAvailable())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSFriends: cannot invite - no Epic social graph for this user"));
			Callback(FGamingServiceResult(false));
			return;
		}

		const FString LobbyId = Matchmaking.GetCurrentLobbyId();
		if (LobbyId.IsEmpty())
		{
			UE_LOG(LogTemp, Error,
			       TEXT("EOSFriends: cannot invite '%s' - not in a lobby, so there is nothing to invite to"),
			       *FriendUserId);
			Callback(FGamingServiceResult(false));
			return;
		}

		EOS_EpicAccountId TargetEpicId = EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*FriendUserId));
		if (!TargetEpicId || EOS_EpicAccountId_IsValid(TargetEpicId) == EOS_FALSE)
		{
			UE_LOG(LogTemp, Error, TEXT("EOSFriends: '%s' is not an EpicAccountId"), *FriendUserId);
			Callback(FGamingServiceResult(false));
			return;
		}

		// Lobby invites address ProductUserIds, but the social graph speaks EpicAccountIds, so the id has
		// to be translated before it can be used. The mapping must be queried before it can be read.
		struct FMapCtx
		{
			FEOSFriends* Owner;
			FString FriendUserId;
			FString LobbyId;
			TFunction<void(const FGamingServiceResult&)> Callback;
		};
		auto* Ctx = new FMapCtx{this, FriendUserId, LobbyId, MoveTemp(Callback)};

		const std::string TargetUtf8 = TCHAR_TO_UTF8(*FriendUserId);
		const char* TargetIds[1] = {TargetUtf8.c_str()};

		EOS_Connect_QueryExternalAccountMappingsOptions MapOptions = {};
		MapOptions.ApiVersion = EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST;
		MapOptions.LocalUserId = Impl->LocalProductUserId();
		MapOptions.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
		MapOptions.ExternalAccountIds = TargetIds;
		MapOptions.ExternalAccountIdCount = 1;

		EOS_Connect_QueryExternalAccountMappings(
			Impl->ConnectHandle(),
			&MapOptions,
			Ctx,
			[](const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data)
			{
				check(Data);
				check(Data->ClientData);
				const TUniquePtr<FMapCtx> LocalCtx(static_cast<FMapCtx*>(Data->ClientData));
				FEOSFriends* Owner = LocalCtx->Owner;

				if (Data->ResultCode != EOS_EResult::EOS_Success)
				{
					UE_LOG(LogTemp, Error, TEXT("EOSFriends: could not map '%s' to a ProductUserId: %s"),
					       *LocalCtx->FriendUserId, *FString(EOS_EResult_ToString(Data->ResultCode)));
					LocalCtx->Callback(FGamingServiceResult(false));
					return;
				}

				const std::string TargetUtf8 = TCHAR_TO_UTF8(*LocalCtx->FriendUserId);

				EOS_Connect_GetExternalAccountMappingsOptions GetOptions = {};
				GetOptions.ApiVersion = EOS_CONNECT_GETEXTERNALACCOUNTMAPPING_API_LATEST;
				GetOptions.LocalUserId = Owner->Impl->LocalProductUserId();
				GetOptions.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
				GetOptions.TargetExternalUserId = TargetUtf8.c_str();

				EOS_ProductUserId TargetPuid =
					EOS_Connect_GetExternalAccountMapping(Owner->Impl->ConnectHandle(), &GetOptions);
				if (!TargetPuid || EOS_ProductUserId_IsValid(TargetPuid) == EOS_FALSE)
				{
					// The friend has an Epic account but has never played this product, so there is no
					// ProductUserId to address. Nothing the caller can fix.
					UE_LOG(LogTemp, Warning,
					       TEXT("EOSFriends: '%s' has no ProductUserId for this product - they have never "
					            "played it, so they cannot be invited"),
					       *LocalCtx->FriendUserId);
					LocalCtx->Callback(FGamingServiceResult(false));
					return;
				}

				const std::string LobbyIdUtf8 = TCHAR_TO_UTF8(*LocalCtx->LobbyId);

				EOS_Lobby_SendInviteOptions InviteOptions = {};
				InviteOptions.ApiVersion = EOS_LOBBY_SENDINVITE_API_LATEST;
				InviteOptions.LobbyId = LobbyIdUtf8.c_str();
				InviteOptions.LocalUserId = Owner->Impl->LocalProductUserId();
				InviteOptions.TargetUserId = TargetPuid;

				struct FInviteCtx
				{
					FString FriendUserId;
					TFunction<void(const FGamingServiceResult&)> Callback;
				};
				auto* InviteCtx = new FInviteCtx{LocalCtx->FriendUserId, MoveTemp(LocalCtx->Callback)};

				EOS_Lobby_SendInvite(
					Owner->Impl->LobbyHandle(),
					&InviteOptions,
					InviteCtx,
					[](const EOS_Lobby_SendInviteCallbackInfo* InviteData)
					{
						check(InviteData);
						check(InviteData->ClientData);
						const TUniquePtr<FInviteCtx> C(static_cast<FInviteCtx*>(InviteData->ClientData));

						const bool bOk = InviteData->ResultCode == EOS_EResult::EOS_Success;
						if (bOk)
						{
							UE_LOG(LogTemp, Log, TEXT("EOSFriends: invited '%s'"), *C->FriendUserId);
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("EOSFriends: invite to '%s' failed: %s"),
							       *C->FriendUserId, *FString(EOS_EResult_ToString(InviteData->ResultCode)));
						}
						C->Callback(FGamingServiceResult(bOk));
					});
			});
	}
}

#endif // GS_WITH_EOS

#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamFriends.h"
#include "Native/Steam/Interfaces/SteamInviteTransport.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	namespace
	{
		/** Steam's persona states collapsed onto the states every backend can report. */
		EGamingFriendState ToFriendState(EPersonaState Persona)
		{
			switch (Persona)
			{
			case k_EPersonaStateOffline:
				return EGamingFriendState::Offline;
			case k_EPersonaStateBusy:
				return EGamingFriendState::Busy;
			case k_EPersonaStateAway:
			case k_EPersonaStateSnooze:
				return EGamingFriendState::Away;
			default:
				// Online, LookingToTrade, LookingToPlay and anything Steam adds later. Defaulting to
				// Online rather than Offline keeps a reachable friend from being hidden by a new enum.
				return EGamingFriendState::Online;
			}
		}
	}

	struct FSteamFriends::FImpl
	{
		FSteamFriends& Owner;

		TArray<FGamingFriend> Cached;

		CCallback<FImpl, PersonaStateChange_t> m_CallbackPersonaStateChange;

		explicit FImpl(FSteamFriends& InOwner)
			: Owner(InOwner)
			, m_CallbackPersonaStateChange(this, &FImpl::OnPersonaStateChange)
		{
		}

		/**
		 * Steam fires this for every persona field it finishes downloading, for friends AND for unrelated
		 * users encountered in lobbies, so it is far chattier than the friend list changing. Filter to the
		 * fields that actually alter what a friend list shows before waking listeners.
		 */
		void OnPersonaStateChange(PersonaStateChange_t* Param)
		{
			if (!Param)
			{
				return;
			}

			const int32 Interesting = k_EPersonaChangeName | k_EPersonaChangeStatus
				| k_EPersonaChangeComeOnline | k_EPersonaChangeGoneOffline | k_EPersonaChangeGamePlayed;
			if ((Param->m_nChangeFlags & Interesting) == 0)
			{
				return;
			}

			// Only re-read if this user is someone we are actually showing.
			const FString ChangedId = FString::Printf(TEXT("%llu"), (uint64)Param->m_ulSteamID);
			const bool bKnown = Cached.ContainsByPredicate(
				[&ChangedId](const FGamingFriend& F) { return F.UserId == ChangedId; });
			if (!bKnown)
			{
				return;
			}

			Refresh();
			if (Owner.OnFriendsChanged)
			{
				Owner.OnFriendsChanged();
			}
		}

		/** Re-read the whole list from Steam. Synchronous; Steam holds this locally. */
		void Refresh()
		{
			Cached.Reset();

			ISteamFriends* Friends = ::SteamFriends();
			ISteamUtils* Utils = ::SteamUtils();
			if (!Friends || !Utils)
			{
				return;
			}

			const uint32 ThisAppId = Utils->GetAppID();
			const int32 Count = Friends->GetFriendCount(k_EFriendFlagImmediate);
			if (Count < 0)
			{
				// Documented as -1 when the current user's friend list is not yet loaded.
				UE_LOG(LogTemp, Verbose, TEXT("SteamFriends: friend list not ready yet"));
				return;
			}

			Cached.Reserve(Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const CSteamID FriendId = Friends->GetFriendByIndex(Index, k_EFriendFlagImmediate);
				if (!FriendId.IsValid())
				{
					continue;
				}

				FGamingFriend Entry;
				Entry.UserId = FString::Printf(TEXT("%llu"), FriendId.ConvertToUint64());

				const char* Persona = Friends->GetFriendPersonaName(FriendId);
				Entry.DisplayName = (Persona && Persona[0] != '\0') ? UTF8_TO_TCHAR(Persona) : Entry.UserId;

				Entry.State = ToFriendState(Friends->GetFriendPersonaState(FriendId));

				// GetFriendGamePlayed is the only way to tell "online" from "online in our game"; it also
				// returns false for an offline friend, so this doubles as the invitability test.
				FriendGameInfo_t GameInfo{};
				if (Friends->GetFriendGamePlayed(FriendId, &GameInfo))
				{
					Entry.bPlayingThisGame = GameInfo.m_gameID.AppID() == ThisAppId;
				}

				Cached.Add(MoveTemp(Entry));
			}

			// Most-useful-first: same game, then online, then alphabetical. Stable so the list does not
			// reshuffle under the player's cursor when one friend's presence changes.
			Cached.StableSort([](const FGamingFriend& A, const FGamingFriend& B)
			{
				if (A.bPlayingThisGame != B.bPlayingThisGame)
				{
					return A.bPlayingThisGame;
				}
				if (A.IsOnline() != B.IsOnline())
				{
					return A.IsOnline();
				}
				return A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase) < 0;
			});
		}
	};

	FSteamFriends::FSteamFriends(FSteamPlatformCore& InCore, FSteamInviteTransport& InInviteTransport)
		: Core(InCore)
		, InviteTransport(InInviteTransport)
		, Impl(MakePimpl<FImpl>(*this))
	{
	}

	FSteamFriends::~FSteamFriends() = default;

	bool FSteamFriends::IsAvailable() const
	{
		return Core.IsInitialized() && Core.IsLoggedIn() && ::SteamFriends() != nullptr;
	}

	void FSteamFriends::QueryFriends(TFunction<void(const FQueryFriendsResult&)> Callback)
	{
		check(Callback);

		if (!IsAvailable())
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamFriends: Steam is not signed in; cannot read the friend list"));
			Callback(FQueryFriendsResult::Failed());
			return;
		}

		Impl->Refresh();

		UE_LOG(LogTemp, Log, TEXT("SteamFriends: read %d friends"), Impl->Cached.Num());

		TArray<FGamingFriend> Copy = Impl->Cached;
		Callback(FQueryFriendsResult::Succeeded(MoveTemp(Copy)));
	}

	const TArray<FGamingFriend>& FSteamFriends::GetCachedFriends() const
	{
		return Impl->Cached;
	}

	void FSteamFriends::SendInvite(const FString& FriendUserId,
	                               TFunction<void(const FGamingServiceResult&)> Callback)
	{
		check(Callback);

		if (!IsAvailable())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamFriends: cannot invite - Steam is not signed in"));
			Callback(FGamingServiceResult(false));
			return;
		}

		// Same payload the overlay's invite button would carry, so both routes join the same session.
		const FString& JoinPayload = InviteTransport.GetJoinPayload();
		if (JoinPayload.IsEmpty())
		{
			UE_LOG(LogTemp, Error,
			       TEXT("SteamFriends: cannot invite '%s' - no joinable session published. Call "
			            "IInviteTransport::SetJoinInfo first."),
			       *FriendUserId);
			Callback(FGamingServiceResult(false));
			return;
		}

		uint64 SteamID64 = 0;
		if (!LexTryParseString(SteamID64, *FriendUserId) || SteamID64 == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamFriends: '%s' is not a SteamID"), *FriendUserId);
			Callback(FGamingServiceResult(false));
			return;
		}

		const CSteamID Target(SteamID64);
		if (!Target.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamFriends: SteamID %llu is not valid"), SteamID64);
			Callback(FGamingServiceResult(false));
			return;
		}

		const FString ConnectString = FString::Printf(TEXT("+connect %s"), *JoinPayload);
		const bool bSent = ::SteamFriends()->InviteUserToGame(Target, TCHAR_TO_UTF8(*ConnectString));
		if (bSent)
		{
			UE_LOG(LogTemp, Log, TEXT("SteamFriends: invited %llu to '%s'"), SteamID64, *JoinPayload);
		}
		else
		{
			// Steam returns false without a reason code. In practice: the target is offline, is not a
			// friend, has invites restricted, or the local user is being rate-limited.
			UE_LOG(LogTemp, Warning, TEXT("SteamFriends: Steam refused the invite to %llu"), SteamID64);
		}

		Callback(FGamingServiceResult(bSent));
	}
}

#endif // GS_WITH_STEAM

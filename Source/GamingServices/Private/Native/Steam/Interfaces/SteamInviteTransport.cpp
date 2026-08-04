#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamInviteTransport.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "Misc/CommandLine.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	namespace
	{
		/** Rich-presence key Steam recognises as "this player is joinable, with this connect string". */
		const char* GConnectPresenceKey = "connect";

		/**
		 * Accepting an invite while the game is closed launches it with "+connect <payload>" instead of
		 * firing a callback. Pull the payload out of the launch command line; empty when absent.
		 *
		 * Deliberately matches "+connect " with the trailing space so it cannot also match Steam's
		 * lobby-invite token "+connect_lobby", which FSteamMatchmaking handles separately.
		 */
		FString ParseConnectPayloadFromCommandLine()
		{
			const FString CmdLine = FCommandLine::Get();
			const TCHAR* Token = TEXT("+connect ");
			const int32 TokenIndex = CmdLine.Find(Token, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			if (TokenIndex == INDEX_NONE)
			{
				return FString();
			}

			FString Remainder = CmdLine.Mid(TokenIndex + FCString::Strlen(Token));
			Remainder.TrimStartInline();

			FString Payload;
			if (!Remainder.Split(TEXT(" "), &Payload, nullptr))
			{
				Payload = Remainder;
			}
			Payload.TrimEndInline();
			Payload.RemoveFromStart(TEXT("\""));
			Payload.RemoveFromEnd(TEXT("\""));

			UE_LOG(LogTemp, Log, TEXT("SteamInviteTransport: [cmdline-join] payload '%s'"), *Payload);
			return Payload;
		}
	}

	struct FSteamInviteTransport::FImpl
	{
		FSteamInviteTransport& Owner;

		/** Published payload, kept so the invite dialog can be opened for it later. */
		FString JoinPayload;

		/** Payload seen at startup or before a listener was bound; delivered by FlushPendingJoin. */
		FString PendingJoinPayload;

		CCallback<FImpl, GameRichPresenceJoinRequested_t> m_CallbackRichPresenceJoin;

		explicit FImpl(FSteamInviteTransport& InOwner)
			: Owner(InOwner)
			, m_CallbackRichPresenceJoin(this, &FImpl::OnRichPresenceJoinRequested)
		{
			PendingJoinPayload = ParseConnectPayloadFromCommandLine();
		}

		void Deliver(const FString& Payload)
		{
			if (Payload.IsEmpty())
			{
				return;
			}
			if (Owner.OnJoinRequested)
			{
				Owner.OnJoinRequested(Payload);
			}
			else
			{
				// Nothing listening yet — hold it rather than drop it.
				PendingJoinPayload = Payload;
			}
		}

		void OnRichPresenceJoinRequested(GameRichPresenceJoinRequested_t* Param)
		{
			if (!Param)
			{
				return;
			}
			const FString Payload = UTF8_TO_TCHAR(Param->m_rgchConnect);
			UE_LOG(LogTemp, Log, TEXT("SteamInviteTransport: join requested with payload '%s'"), *Payload);
			Deliver(Payload);
		}
	};

	FSteamInviteTransport::FSteamInviteTransport(FSteamPlatformCore& InCore)
		: Core(InCore)
		, Impl(MakePimpl<FImpl>(*this))
	{
	}

	FSteamInviteTransport::~FSteamInviteTransport()
	{
		ClearJoinInfo();
	}

	bool FSteamInviteTransport::IsAvailable() const
	{
		return Core.IsInitialized() && Core.IsLoggedIn() && SteamFriends() != nullptr;
	}

	void FSteamInviteTransport::SetJoinInfo(const FString& JoinPayload)
	{
		if (!IsAvailable())
		{
			return;
		}

		Impl->JoinPayload = JoinPayload;
		if (JoinPayload.IsEmpty())
		{
			ClearJoinInfo();
			return;
		}

		// Steam prefixes nothing of its own — whatever is set here is handed back verbatim, both to
		// GameRichPresenceJoinRequested_t and on the launch command line, so it must carry its own token.
		const FString ConnectString = FString::Printf(TEXT("+connect %s"), *JoinPayload);
		if (SteamFriends()->SetRichPresence(GConnectPresenceKey, TCHAR_TO_UTF8(*ConnectString)))
		{
			UE_LOG(LogTemp, Log, TEXT("SteamInviteTransport: published joinable session '%s'"), *JoinPayload);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamInviteTransport: Steam refused the rich-presence connect string"));
		}
	}

	void FSteamInviteTransport::ClearJoinInfo()
	{
		Impl->JoinPayload.Reset();
		if (SteamFriends())
		{
			// Clearing the key is what removes "Join Game" from the friends list.
			SteamFriends()->SetRichPresence(GConnectPresenceKey, nullptr);
		}
	}

	void FSteamInviteTransport::ShowInviteDialog(TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (!IsAvailable() || Impl->JoinPayload.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("SteamInviteTransport: cannot show the invite dialog - no session published"));
			if (Callback)
			{
				Callback(FGamingServiceResult(false));
			}
			return;
		}

		const FString ConnectString = FString::Printf(TEXT("+connect %s"), *Impl->JoinPayload);
		SteamFriends()->ActivateGameOverlayInviteDialogConnectString(TCHAR_TO_UTF8(*ConnectString));

		if (Callback)
		{
			Callback(FGamingServiceResult(true));
		}
	}

	const FString& FSteamInviteTransport::GetJoinPayload() const
	{
		return Impl->JoinPayload;
	}

	void FSteamInviteTransport::FlushPendingJoin()
	{
		if (Impl->PendingJoinPayload.IsEmpty() || !OnJoinRequested)
		{
			return;
		}
		const FString Payload = MoveTemp(Impl->PendingJoinPayload);
		Impl->PendingJoinPayload.Reset();
		UE_LOG(LogTemp, Log, TEXT("SteamInviteTransport: delivering deferred invite payload '%s'"), *Payload);
		OnJoinRequested(Payload);
	}
}

#endif // GS_WITH_STEAM

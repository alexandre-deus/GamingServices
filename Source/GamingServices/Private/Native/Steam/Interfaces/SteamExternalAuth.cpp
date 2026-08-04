#ifdef GS_WITH_STEAM

#include "Native/Steam/Interfaces/SteamExternalAuth.h"
#include "Native/Steam/SteamPlatformCore.h"

#include "Misc/ConfigCacheIni.h"

#include "steam/steam_api.h"

namespace GamingServices
{
	namespace
	{
		/** Identity Epic documents for the Steam identity provider; overridable in Game.ini. */
		const TCHAR* DefaultWebApiIdentity = TEXT("epiconlineservices");

		FString GetWebApiIdentity()
		{
			FString Identity;
			if (GConfig->GetString(TEXT("GamingServices.Steamworks"), TEXT("WebApiIdentity"), Identity, GGameIni) &&
				!Identity.IsEmpty())
			{
				return Identity;
			}
			return DefaultWebApiIdentity;
		}
	}

	struct FSteamExternalAuth::FImpl
	{
		using FCredentialCallback = TFunction<void(const FGamingServiceResult&, const FExternalAuthCredential&)>;

		// Outstanding ticket requests, keyed by the handle Steam gave us so a response is matched to the
		// request that asked for it.
		TMap<HAuthTicket, FCredentialCallback> PendingRequests;

		// Handles minted and not yet cancelled, so ReleaseCredential can retire them.
		TArray<HAuthTicket> IssuedTickets;

		CCallback<FImpl, GetTicketForWebApiResponse_t> m_CallbackWebApiTicket;

		FImpl()
			: m_CallbackWebApiTicket(this, &FImpl::OnWebApiTicket)
		{
		}

		void OnWebApiTicket(GetTicketForWebApiResponse_t* Response)
		{
			if (!Response)
			{
				return;
			}

			FCredentialCallback Callback;
			if (!PendingRequests.RemoveAndCopyValue(Response->m_hAuthTicket, Callback) || !Callback)
			{
				// A ticket somebody else in the process requested.
				return;
			}

			if (Response->m_eResult != k_EResultOK || Response->m_cubTicket <= 0)
			{
				UE_LOG(LogTemp, Error, TEXT("SteamExternalAuth: web-api ticket request failed (result %d, %d bytes)"),
				       (int32)Response->m_eResult, Response->m_cubTicket);
				IssuedTickets.Remove(Response->m_hAuthTicket);
				Callback(FGamingServiceResult(false), FExternalAuthCredential());
				return;
			}

			FExternalAuthCredential Credential;
			Credential.Type = EExternalCredentialType::SteamSessionTicket;
			// EOS takes the ticket as a hex-encoded string, not raw bytes.
			Credential.Token = BytesToHex(Response->m_rgubTicket, Response->m_cubTicket);

			if (SteamUser())
			{
				Credential.NativeUserId = FString::Printf(TEXT("%llu"), SteamUser()->GetSteamID().ConvertToUint64());
			}
			if (SteamFriends())
			{
				const char* PersonaName = SteamFriends()->GetPersonaName();
				Credential.DisplayName = PersonaName ? UTF8_TO_TCHAR(PersonaName) : FString();
			}

			UE_LOG(LogTemp, Log, TEXT("SteamExternalAuth: minted session ticket for %s (%s, %d bytes)"),
			       *Credential.DisplayName, *Credential.NativeUserId, Response->m_cubTicket);

			Callback(FGamingServiceResult(true), Credential);
		}
	};

	FSteamExternalAuth::FSteamExternalAuth(FSteamPlatformCore& InCore)
		: Core(InCore)
		, Impl(MakePimpl<FImpl>())
	{
	}

	FSteamExternalAuth::~FSteamExternalAuth()
	{
		ReleaseCredential();
	}

	bool FSteamExternalAuth::IsReady() const
	{
		return Core.IsInitialized() && Core.IsLoggedIn() && SteamUser() != nullptr;
	}

	void FSteamExternalAuth::RequestCredential(
		TFunction<void(const FGamingServiceResult&, const FExternalAuthCredential&)> Callback)
	{
		check(Callback);

		if (!IsReady())
		{
			UE_LOG(LogTemp, Warning, TEXT("SteamExternalAuth: Steam is not signed in; cannot mint a credential"));
			Callback(FGamingServiceResult(false), FExternalAuthCredential());
			return;
		}

		const FString Identity = GetWebApiIdentity();
		const HAuthTicket Ticket = SteamUser()->GetAuthTicketForWebApi(TCHAR_TO_UTF8(*Identity));
		if (Ticket == k_HAuthTicketInvalid)
		{
			UE_LOG(LogTemp, Error, TEXT("SteamExternalAuth: GetAuthTicketForWebApi('%s') returned no ticket"), *Identity);
			Callback(FGamingServiceResult(false), FExternalAuthCredential());
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SteamExternalAuth: requested web-api ticket for identity '%s'"), *Identity);
		Impl->IssuedTickets.AddUnique(Ticket);
		Impl->PendingRequests.Add(Ticket, MoveTemp(Callback));
	}

	void FSteamExternalAuth::ReleaseCredential()
	{
		if (SteamUser())
		{
			for (const HAuthTicket Ticket : Impl->IssuedTickets)
			{
				SteamUser()->CancelAuthTicket(Ticket);
			}
		}
		Impl->IssuedTickets.Reset();

		// Anything still outstanding will never be answered now; fail those callers rather than strand them.
		TMap<HAuthTicket, FImpl::FCredentialCallback> Pending = MoveTemp(Impl->PendingRequests);
		Impl->PendingRequests.Reset();
		for (auto& Entry : Pending)
		{
			if (Entry.Value)
			{
				Entry.Value(FGamingServiceResult(false), FExternalAuthCredential());
			}
		}
	}
}

#endif // GS_WITH_STEAM

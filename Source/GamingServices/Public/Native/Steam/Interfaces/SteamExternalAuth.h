#pragma once

#ifdef GS_WITH_STEAM

#include "CoreMinimal.h"
#include "Native/Interfaces/IExternalAuthService.h"
#include "Templates/PimplPtr.h"

namespace GamingServices
{
	class FSteamPlatformCore;


	/**
	 * Steam as an identity provider for another backend.
	 *
	 * Mints the web-API auth ticket that EOS Connect accepts (EOS_ECT_STEAM_SESSION_TICKET), which is
	 * what lets a player sign in through the Steam client they already launched from and still have the
	 * whole session — lobbies, P2P, stats, cloud saves — run on EOS.
	 *
	 * The ticket is requested with ISteamUser::GetAuthTicketForWebApi and arrives asynchronously in
	 * GetTicketForWebApiResponse_t; the CCallback and the outstanding requests live in a private FImpl
	 * so this header stays free of the Steam SDK.
	 *
	 * The identity string passed to Steam must match the one configured on the Steam identity provider
	 * in the EOS Dev Portal. It defaults to Epic's documented "epiconlineservices" and can be overridden
	 * with [GamingServices.Steamworks] WebApiIdentity in Game.ini.
	 */
	class FSteamExternalAuth final : public IExternalAuthProvider
	{
	public:
		explicit FSteamExternalAuth(FSteamPlatformCore& InCore);
		virtual ~FSteamExternalAuth() override;

		virtual EExternalCredentialType GetProvidedCredentialType() const override
		{
			return EExternalCredentialType::SteamSessionTicket;
		}

		virtual bool IsReady() const override;

		virtual void RequestCredential(
			TFunction<void(const FGamingServiceResult&, const FExternalAuthCredential&)> Callback) override;

		virtual void ReleaseCredential() override;

	private:
		struct FImpl;

		FSteamPlatformCore& Core;
		TPimplPtr<FImpl> Impl;
	};
}

#endif // GS_WITH_STEAM

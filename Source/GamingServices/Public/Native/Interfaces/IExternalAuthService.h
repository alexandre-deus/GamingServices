#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"

/**
 * Kind of proof-of-identity one backend can hand to another.
 *
 * The value names the credential, not the backend that minted it, because the consumer cares only
 * about the format: EOS Connect accepts a Steam session ticket regardless of who produced it.
 */
enum class EExternalCredentialType : uint8
{
	None,

	/** Steam ticket from ISteamUser::GetAuthTicketForWebApi, hex-encoded. EOS: EOS_ECT_STEAM_SESSION_TICKET. */
	SteamSessionTicket,

	/** Steam encrypted app ticket, hex-encoded. EOS: EOS_ECT_STEAM_APP_TICKET. Deprecated by Epic in favour of the session ticket. */
	SteamAppTicket,

	/** Epic access token from the EOS Auth interface. EOS: EOS_ECT_EPIC. */
	EpicAccessToken,
};

inline const TCHAR* LexToString(EExternalCredentialType Type)
{
	switch (Type)
	{
	case EExternalCredentialType::SteamSessionTicket: return TEXT("SteamSessionTicket");
	case EExternalCredentialType::SteamAppTicket:     return TEXT("SteamAppTicket");
	case EExternalCredentialType::EpicAccessToken:    return TEXT("EpicAccessToken");
	default:                                          return TEXT("None");
	}
}

/** A single-use proof of identity minted by one backend for another to log in with. */
struct GAMINGSERVICES_API FExternalAuthCredential
{
	EExternalCredentialType Type = EExternalCredentialType::None;

	/** The credential itself, already in the encoding the consumer expects (hex for Steam tickets). */
	FString Token;

	/** The minting platform's own id for the user (e.g. SteamID64). Informational. */
	FString NativeUserId;

	/**
	 * The minting platform's display name. Consumers pass this along as non-authoritative user info
	 * (EOS stores it on the product user so leaderboards and lobbies have a name to show), which is
	 * what keeps Steam personas visible on an EOS-run session.
	 */
	FString DisplayName;

	bool IsValid() const { return Type != EExternalCredentialType::None && !Token.IsEmpty(); }
};

/**
 * A backend that can vouch for the local user to somebody else — the "sign in with Steam" half of a
 * cross-backend login.
 *
 * Implemented by the backend whose client is already signed in (Steam). Exposed from
 * IGamingService::GetExternalAuthProvider().
 */
class GAMINGSERVICES_API IExternalAuthProvider
{
public:
	virtual ~IExternalAuthProvider() = default;

	/** Credential format this provider mints. */
	virtual EExternalCredentialType GetProvidedCredentialType() const = 0;

	/** Whether the platform is up and the user signed in, so a credential can be requested at all. */
	virtual bool IsReady() const = 0;

	/**
	 * Asynchronously mint a credential. The callback fires exactly once; on failure the credential is
	 * invalid and the caller should fall back to the consumer's own native login.
	 *
	 * Tickets are short-lived and single-purpose — request one per login attempt rather than caching.
	 */
	virtual void RequestCredential(TFunction<void(const FGamingServiceResult&, const FExternalAuthCredential&)> Callback) = 0;

	/** Retire the last minted credential (Steam: CancelAuthTicket). Safe to call when none is outstanding. */
	virtual void ReleaseCredential() {}
};

/**
 * A backend that can be logged into with somebody else's credential — the "...and run everything on
 * EOS" half.
 *
 * Implemented by the backend that owns the session (EOS, via Connect). Exposed from
 * IGamingService::GetExternalAuthConsumer().
 */
class GAMINGSERVICES_API IExternalAuthConsumer
{
public:
	virtual ~IExternalAuthConsumer() = default;

	/** Whether this backend can log in with the given credential format. */
	virtual bool SupportsCredentialType(EExternalCredentialType Type) const = 0;

	/**
	 * Log in using a credential minted elsewhere, creating the local account on first sight if the
	 * platform requires it. On success the backend is fully logged in and every one of its capabilities
	 * is usable, exactly as after its own native login.
	 */
	virtual void LoginWithExternalCredential(const FExternalAuthCredential& Credential,
	                                         TFunction<void(const FGamingServiceResult&)> Callback) = 0;
};

#include "Native/Composite/CompositeUser.h"

#include "Native/IGamingService.h"
#include "Native/Interfaces/IExternalAuthService.h"

namespace GamingServices
{
	FCompositeUser::FCompositeUser(IGamingService& InSession, IGamingService* InIdentity, bool bInAllowFallback)
		: Session(InSession)
		, Identity(InIdentity)
		, bAllowFallback(bInAllowFallback)
	{
		// Avatar readiness is produced by the identity backend but observed on this object, so forward it.
		if (IUserService* IdentityUser = GetIdentityUser())
		{
			IdentityUser->OnAvatarReady = [this](const FString& UserId)
			{
				if (OnAvatarReady)
				{
					OnAvatarReady(UserId);
				}
			};
		}
	}

	FCompositeUser::~FCompositeUser()
	{
		// The sub-services outlive this object during teardown; drop the sink so it cannot fire into a
		// destroyed composite.
		if (IUserService* IdentityUser = GetIdentityUser())
		{
			IdentityUser->OnAvatarReady = nullptr;
		}
	}

	IUserService* FCompositeUser::GetSessionUser() const
	{
		return Session.GetUser();
	}

	IUserService* FCompositeUser::GetIdentityUser() const
	{
		return Identity ? Identity->GetUser() : nullptr;
	}

	void FCompositeUser::LoginWithSessionBackend(const FGamingServiceLoginParams& Params,
	                                             TFunction<void(const FGamingServiceResult&)> Callback)
	{
		if (IUserService* SessionUser = GetSessionUser())
		{
			SessionUser->Login(Params, MoveTemp(Callback));
			return;
		}

		UE_LOG(LogTemp, Error, TEXT("CompositeUser: the session backend exposes no user capability"));
		Callback(FGamingServiceResult(false));
	}

	void FCompositeUser::Login(const FGamingServiceLoginParams& Params,
	                           TFunction<void(const FGamingServiceResult&)> Callback)
	{
		check(Callback);

		IExternalAuthProvider* Provider = Identity ? Identity->GetExternalAuthProvider() : nullptr;
		IExternalAuthConsumer* Consumer = Session.GetExternalAuthConsumer();

		const TCHAR* Unavailable = nullptr;
		if (!Provider)
		{
			Unavailable = TEXT("the identity backend cannot mint credentials");
		}
		else if (!Consumer)
		{
			Unavailable = TEXT("the session backend cannot consume external credentials");
		}
		else if (!Consumer->SupportsCredentialType(Provider->GetProvidedCredentialType()))
		{
			Unavailable = TEXT("the session backend does not accept this credential format");
		}
		else if (!Provider->IsReady())
		{
			Unavailable = TEXT("the identity backend is not signed in");
		}

		if (Unavailable)
		{
			if (!bAllowFallback)
			{
				UE_LOG(LogTemp, Error, TEXT("CompositeUser: cross-backend login unavailable (%s) and fallback is disabled"),
				       Unavailable);
				Callback(FGamingServiceResult(false));
				return;
			}

			UE_LOG(LogTemp, Warning, TEXT("CompositeUser: cross-backend login unavailable (%s); using the session backend's own login"),
			       Unavailable);
			LoginWithSessionBackend(Params, MoveTemp(Callback));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("CompositeUser: requesting a %s credential to sign in to %s"),
		       LexToString(Provider->GetProvidedCredentialType()),
		       GamingServices::LexToString(Session.GetBackend()));

		Provider->RequestCredential(
			[this, Params, Callback = MoveTemp(Callback), Consumer](const FGamingServiceResult& Result,
			                                                        const FExternalAuthCredential& Credential) mutable
			{
				if (!Result.bSuccess || !Credential.IsValid())
				{
					if (!bAllowFallback)
					{
						UE_LOG(LogTemp, Error, TEXT("CompositeUser: could not obtain a credential and fallback is disabled"));
						Callback(FGamingServiceResult(false));
						return;
					}

					UE_LOG(LogTemp, Warning,
					       TEXT("CompositeUser: could not obtain a credential; using the session backend's own login"));
					LoginWithSessionBackend(Params, MoveTemp(Callback));
					return;
				}

				// Keep the identity-side details: after this point the session backend owns the login, but
				// the player's name and avatar still come from the platform that vouched for them.
				IdentityDisplayName = Credential.DisplayName;
				IdentityNativeUserId = Credential.NativeUserId;

				const EExternalCredentialType CredentialType = Credential.Type;
				Consumer->LoginWithExternalCredential(Credential,
					[this, Params, CredentialType, Callback = MoveTemp(Callback)](const FGamingServiceResult& LoginResult) mutable
					{
						// Falling back matters most HERE, not just when the credential could not be minted:
						// a well-formed ticket the session backend refuses (identity provider not set up in
						// the portal, credential type not enabled, backend outage) is exactly the case that
						// otherwise leaves the game running with nobody logged in.
						if (LoginResult.bSuccess || !bAllowFallback)
						{
							Callback(LoginResult);
							return;
						}

						UE_LOG(LogTemp, Warning,
						       TEXT("CompositeUser: %s refused the %s credential; using its own login instead"),
						       GamingServices::LexToString(Session.GetBackend()),
						       LexToString(CredentialType));
						LoginWithSessionBackend(Params, MoveTemp(Callback));
					});
			});
	}

	bool FCompositeUser::IsLoggedIn() const
	{
		IUserService* SessionUser = GetSessionUser();
		return SessionUser && SessionUser->IsLoggedIn();
	}

	bool FCompositeUser::NeedsLogin() const
	{
		IUserService* SessionUser = GetSessionUser();
		return SessionUser && SessionUser->NeedsLogin();
	}

	FString FCompositeUser::GetUserId() const
	{
		// Always the session backend's id: it is what every other capability keys on.
		IUserService* SessionUser = GetSessionUser();
		return SessionUser ? SessionUser->GetUserId() : FString();
	}

	FString FCompositeUser::GetDisplayName() const
	{
		if (!IdentityDisplayName.IsEmpty())
		{
			return IdentityDisplayName;
		}
		if (IUserService* IdentityUser = GetIdentityUser())
		{
			const FString Name = IdentityUser->GetDisplayName();
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}
		IUserService* SessionUser = GetSessionUser();
		return SessionUser ? SessionUser->GetDisplayName() : FString();
	}

	void FCompositeUser::ResolveDisplayName(const FString& UserId,
	                                        TFunction<void(const FResolveDisplayNameResult&)> Callback)
	{
		check(Callback);

		// Remote ids are session-backend ids, so it is the only one that can resolve them. It carries the
		// display name each member supplied at login, which is how Steam personas survive into an EOS session.
		if (IUserService* SessionUser = GetSessionUser())
		{
			SessionUser->ResolveDisplayName(UserId, MoveTemp(Callback));
			return;
		}

		// No session user at all — hand back the contract's documented fallback rather than stranding the caller.
		FResolveDisplayNameResult Fallback;
		Fallback.UserId = UserId;
		Fallback.DisplayName = UserId;
		Callback(Fallback);
	}

	UTexture2D* FCompositeUser::GetAvatar() const
	{
		// The local player's avatar comes from the platform they signed in with (EOS has no avatar API).
		if (IUserService* IdentityUser = GetIdentityUser())
		{
			if (UTexture2D* Avatar = IdentityUser->GetAvatar())
			{
				return Avatar;
			}
		}
		IUserService* SessionUser = GetSessionUser();
		return SessionUser ? SessionUser->GetAvatar() : nullptr;
	}

	UTexture2D* FCompositeUser::GetAvatarByUserId(const FString& UserId) const
	{
		// The local user is reachable under either backend's id.
		if (!UserId.IsEmpty() && (UserId == GetUserId() || UserId == IdentityNativeUserId))
		{
			if (UTexture2D* Avatar = GetAvatar())
			{
				return Avatar;
			}
		}

		if (IUserService* SessionUser = GetSessionUser())
		{
			if (UTexture2D* Avatar = SessionUser->GetAvatarByUserId(UserId))
			{
				return Avatar;
			}
		}

		// Last resort: the id may be a native identity-backend id (a SteamID64 passed straight through).
		// Remote session ids will not resolve here, and correctly return null rather than a wrong face.
		if (IUserService* IdentityUser = GetIdentityUser())
		{
			return IdentityUser->GetAvatarByUserId(UserId);
		}
		return nullptr;
	}
}

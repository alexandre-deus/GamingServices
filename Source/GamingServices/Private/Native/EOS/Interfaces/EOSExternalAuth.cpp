#if defined(GS_WITH_EOS)

#include "Native/EOS/Interfaces/EOSExternalAuth.h"
#include "Native/EOS/EOSPlatformCore.h"

namespace GamingServices
{
	bool FEOSExternalAuth::SupportsCredentialType(EExternalCredentialType Type) const
	{
		return FEOSPlatformCore::SupportsExternalCredential(Type);
	}

	void FEOSExternalAuth::LoginWithExternalCredential(const FExternalAuthCredential& Credential,
	                                                   TFunction<void(const FGamingServiceResult&)> Callback)
	{
		check(Callback);

		if (!Credential.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("EOSExternalAuth: refused an invalid credential"));
			Callback(FGamingServiceResult(false));
			return;
		}

		Core.LoginWithExternalCredential(Credential.Type, Credential.Token, Credential.DisplayName, MoveTemp(Callback));
	}
}

#endif // GS_WITH_EOS

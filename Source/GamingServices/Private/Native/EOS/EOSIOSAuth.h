#pragma once

#include "CoreMinimal.h"

#if defined(GS_WITH_EOS) && PLATFORM_IOS

namespace GamingServices
{
	/**
	 * The iOS half of EOS_Auth_Credentials, as an opaque pointer for its SystemAuthCredentialsOptions
	 * field (declared void* by the SDK precisely so callers need not see the platform struct).
	 *
	 * Why iOS needs this and no other platform does: the account portal is a browser either way, but on
	 * iOS the SDK presents it through ASWebAuthenticationSession, and since multi-scene support Apple
	 * requires the app to name the window to present over — UIApplication has no dependable "current
	 * window" left to infer. Android hands the SDK an Activity context up front (EOSSDK.init, wired by
	 * EOS_Android_UPL.xml) so nothing has to be passed per login; desktop opens a real browser and has
	 * no anchor to speak of.
	 *
	 * Defined in EOSIOSAuth.mm because the declaring SDK header, eos_IOS.h, imports UIKit and so cannot
	 * be included from a C++ translation unit.
	 *
	 * Call on the game thread, once per login attempt, and pass the result straight to EOS_Auth_Login.
	 */
	void* GetIOSAuthCredentialsOptions();
}

#endif // GS_WITH_EOS && PLATFORM_IOS

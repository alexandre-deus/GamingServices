// Lives in an IOS/ directory so UBT compiles it only when targeting iOS. Android builds otherwise try
// to compile this .mm and clang rejects it with "Objective-C was disabled in PCH file but is currently
// enabled" — the internal PLATFORM_IOS guard cannot help, because that failure happens before the
// preprocessor empties the file. The header stays one level up: it is fully guarded and harmless to
// include from anywhere.
#include "../EOSIOSAuth.h"

#if defined(GS_WITH_EOS) && PLATFORM_IOS

#import <UIKit/UIKit.h>
#import <AuthenticationServices/AuthenticationServices.h>

#include "IOS/IOSAppDelegate.h"

// The one place eos_IOS.h is included. It imports UIKit, so it only compiles as Objective-C++ — which
// is the whole reason this file exists as a .mm rather than living in EOSPlatformCore.cpp.
#include "eos_IOS.h"

/**
 * Supplies the window the account portal is presented over.
 *
 * ASWebAuthenticationSession asks its context provider for an anchor at the moment it presents, rather
 * than being handed one up front, so this has to be a live object that outlives the EOS_Auth_Login call
 * — it is consulted later, when the SDK actually opens the session.
 */
@interface GSEOSPresentationContext : NSObject <ASWebAuthenticationPresentationContextProviding>
@end

@implementation GSEOSPresentationContext

- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession*)Session
{
	// The engine's own UIWindow. Reading it at presentation time (rather than caching one) keeps this
	// correct across a window the engine recreates, e.g. after a resolution or orientation change.
	return [IOSAppDelegate GetDelegate].Window;
}

@end

namespace GamingServices
{
	void* GetIOSAuthCredentialsOptions()
	{
		// EOS copies the options struct during the synchronous part of EOS_Auth_Login, so the struct
		// itself only has to survive that call. A static keeps the returned pointer valid without
		// allocating one per attempt; logins are issued from the game thread, one at a time.
		static EOS_IOS_Auth_CredentialsOptions Options = {};

		Options.ApiVersion = EOS_IOS_AUTH_CREDENTIALSOPTIONS_API_LATEST;

		// Ownership of this reference passes to the SDK, which releases it once the value is consumed
		// (iOS 13+). That is why a fresh retained instance is handed over per login instead of one
		// shared for the life of the process — reusing it would hand the SDK an over-released object.
		//
		// The SDK must end up holding exactly one reference, and what it takes to get there differs by
		// memory model: under ARC, CFBridgingRetain consumes the local and yields the +1 the SDK will
		// release. Without ARC — the engine's default for this module — CFBridgingRetain is a plain
		// CFRetain on top of the +1 from alloc/init, so this owns a reference that has to be given up.
		GSEOSPresentationContext* Context = [[GSEOSPresentationContext alloc] init];
		Options.PresentationContextProviding = (void*)CFBridgingRetain(Context);
#if !__has_feature(objc_arc)
		[Context release];
#endif

		// Optional. Only used to render a placeholder if the app is backgrounded while the portal is
		// showing; leaving it null just means the default snapshot behaviour.
		Options.CreateBackgroundSnapshotView = nullptr;
		Options.CreateBackgroundSnapshotViewContext = nullptr;

		return &Options;
	}
}

#endif // GS_WITH_EOS && PLATFORM_IOS

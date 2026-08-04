#pragma once

#if defined(GS_WITH_EOS)

#include "CoreMinimal.h"

// The EOS SDK headers, included from one private place. Nothing else in this module includes an
// eos_*.h directly: the runtime symbol table (EOSDynamicApi.h) is built with decltype over these
// declarations, and the redirect header that follows it rewrites every call onto that table.
#include "eos_sdk.h"
#include "eos_common.h"
#include "eos_auth.h"
#include "eos_achievements.h"
#include "eos_stats.h"
#include "eos_leaderboards.h"
#include "eos_connect.h"
#include "eos_logging.h"
#include "eos_playerdatastorage.h"
#include "eos_lobby.h"
#include "eos_ecom.h"
#include "eos_userinfo.h"
#include "eos_p2p.h"
#include "eos_friends.h"
#include "eos_presence.h"

#if PLATFORM_ANDROID
#include "Android/eos_Android.h"
#endif

#endif // GS_WITH_EOS

## GamingServices Plugin — 2.0

One capability-oriented abstraction over Epic Online Services and Steamworks, plus a P2P net driver
that runs Unreal replication over either platform's relay.

Every vendored SDK is **compiled in and dynamically bound** — no import library, no delay-load entry.
Which backend actually runs, and how several are wired together, is a **profile declared in code**, so a
build variant is a one-token change and a missing SDK is a runtime non-event rather than a link error.

### What 2.0 changed

2.0 replaced the single fat `UGamingServicesSubsystem` (one class holding every operation and event,
with `Connect(...)` called by the game) with:

- **One interface per capability** (`IAchievementsService`, `IMatchmakingService`, `IUserService`, …).
  A backend implements what it can; everything else stays null, so "unsupported" is a null check at the
  source instead of a runtime error string. `GetCapabilities()` is *derived* from those accessors, so the
  capability report and the code can never disagree.
- **Backend arrangements as data** (`Native/GamingServiceProfile.h`) instead of build-time `#if` walls —
  including "sign in on one platform, run the session on another".
- **Per-capability Blueprint libraries** of async-action nodes (`Blueprint/Libraries/`) instead of one
  subsystem surface. `UGamingPlatformSubsystem` now only ticks the service and answers capability queries.
- **Platform lifecycle owned by the module**, not the game: `FGamingServicesModule::StartupModule` builds
  and initialises exactly one service from ini. Game code no longer calls `Connect(...)` at all.
- **A P2P net driver** (`UMinderaNetDriver`) over an SDK-free `IP2PTransport`, so Steam and EOS relays are
  the same code path to the engine.

### Capabilities

Achievements · Entitlements · Leaderboards · Stats · Cloud storage · Remote settings · Matchmaking /
lobbies · User identity & avatars · Friends · Invites · P2P transport

---

## 1) Requirements

- Unreal Engine 5.7 (developed against 5.7; the netdriver and config layers are the version-sensitive parts)
- `OnlineSubsystemUtils` enabled (the plugin depends on it for the `UIpNetDriver` base)
- Platform SDKs, vendored by you under this plugin's `ThirdParty/` (never committed)

Backend availability per platform is decided by which SDK binary exists for that platform:

| Platform | EOS | Steamworks |
|---|---|---|
| Win64 | `EOSSDK-Win64-Shipping.dll` | `steam_api64.dll` |
| Mac | `libEOSSDK-Mac-Shipping.dylib` | `libsteam_api.dylib` |
| Linux / LinuxArm64 | `libEOSSDK-Linux(Arm64)-Shipping.so` | `libsteam_api.so` (Linux only) |
| Android | `libEOSSDK.so`, from the separate Android SDK | — |
| iOS | `EOSSDK.framework`, from the separate iOS SDK | — |

A platform with no backend compiled in gets the null service: the game starts, and every capability
honestly reports unsupported.

---

## 2) SDK Download and Placement

Download the SDKs and place them inside the plugin `ThirdParty` directory with the following structure:

EOS ships **three separate downloads** — desktop, Android and iOS — which are different releases of
different shapes and are not interchangeable. Each gets its own directory. Steamworks is desktop-only.

```
GamingServices/
  ThirdParty/
    EOS/
      SDK/                  # desktop EOS SDK (Windows/Linux/Mac)
        Include/
        Lib/
        Bin/
      SDK-Android/          # Android EOS SDK (separate download; AAR-based)
        Include/
        Bin/
          Android/
            static-stdc++/
              aar/          # eossdk-StaticSTDC-release.aar
        Tools/              # dev auth tool etc.; unused by the build
      SDK-IOS/              # iOS EOS SDK (separate download; framework-based)
        Bin/
          EOSSDK.framework/
            Headers/        # the iOS headers live here — no sibling Include/ on this platform
          EOSSDK.xcframework/
          EOSSDK.embeddedframework.zip   # ⚠ not in the download — you build this, see §2.1
    Steamworks/
      sdk/
        public/
        redistributable_bin/
        win64/            # present in Steam package (contains steam_api64.dll)
```

Every tree above is a **verbatim extraction** of the vendor download. Nothing is copied, moved or
renamed afterwards. The single exception is the line marked ⚠: UBT needs the iOS framework repackaged
as a zip, and no such archive ships in the download.

Note that iOS has no `Include/`. Its headers ship inside the framework, and the build points the
include path straight at `Bin/EOSSDK.framework/Headers` rather than copying them out to match the
other platforms — so an SDK upgrade is a re-extract with no fixup step to forget. Include style is
unchanged (`#include "eos_sdk.h"`); only the directory differs.

### 2.1) Epic Online Services (EOS)
- Download: `https://dev.epicgames.com/docs` (navigate to Epic Online Services SDK)
- Place the **desktop** SDK so the include/lib/bin paths match:
  - `ThirdParty/EOS/SDK/Include`
  - `ThirdParty/EOS/SDK/Lib`
  - `ThirdParty/EOS/SDK/Bin`

> No import library is needed. The SDK is loaded and bound at runtime (see §3.7), so `Lib/` is
> unused — only `Include/` and `Bin/` matter.

For **Android**, download the separate "EOS SDK for Android" package and extract it under `SDK-Android/`
(it is a different, usually newer, release than the desktop SDK and is packaged as an AAR):
- `ThirdParty/EOS/SDK-Android/Include`
- `ThirdParty/EOS/SDK-Android/Bin/Android/static-stdc++/aar/eossdk-StaticSTDC-release.aar`

Nothing is linked and nothing is staged on Android. The AAR (runtime `.so` + JNI classes) is packaged
into the APK by `EOS_Android_UPL.xml`, which also wires the SDK's Java bootstrap (`EOSSDK.init`),
Java 8 desugaring, and the `eos.<clientid>` deep-link scheme that Account Portal / Persistent Auth
logins redirect back through. At runtime the loader just `dlopen()`s the already-packaged library by
name. The extracted tree is used as-is.

For **iOS**, download the separate "EOS SDK for iOS" package and extract it under `SDK-IOS/`. Leave the
extracted tree exactly as it comes — in particular there is **no `Include/` folder on this platform**.
The iOS SDK carries its headers inside the framework, and the build reads them there:

```
ThirdParty/EOS/SDK-IOS/Bin/EOSSDK.framework/Headers/
```

`GamingServices.Build.cs` puts that directory on the include path for iOS targets (`EOSIncludeDir`)
instead of the sibling `Include/` used everywhere else. The headers are not copied out to make the
tree uniform: a verbatim extraction means an SDK upgrade is a re-extract with no manual fixup step
that can be forgotten or done wrong. Nothing about include style changes — the EOS headers are flat
and refer to each other unqualified (`#include "eos_sdk.h"`), so only the directory differs.

One thing does have to be produced by hand: **build `EOSSDK.embeddedframework.zip`.** UBT's `Framework`
helper takes a framework as a zip laid out `<Name>.embeddedframework/<Name>.framework`, and no such
archive ships in the download. Wrap the framework in that directory and zip it:

```bash
cd ThirdParty/EOS/SDK-IOS/Bin
mkdir -p EOSSDK.embeddedframework
cp -R EOSSDK.framework EOSSDK.embeddedframework/
zip -ry EOSSDK.embeddedframework.zip EOSSDK.embeddedframework
rm -rf EOSSDK.embeddedframework      # only the zip is consumed
```

The framework itself is unmodified — it is repackaged, not rebuilt or re-signed. If the zip is absent
the build fails the backend with
`ERROR: EOS iOS framework missing: …` and produces a client with no EOS at all.

> **Why iOS is different.** Every other platform loads its EOS binary at runtime, but
> `FPlatformProcess::GetDllHandle` is unimplemented and fatal on iOS, so there is nothing to stage into
> the common folder and `dlopen()`. Instead the framework is linked and copied into the `.app`
> (`Framework.FrameworkMode.LinkAndCopy`): dyld maps it before `main()`, and `FGamingSdkLibrary`
> resolves the same symbol table straight out of the process image. No call site changes.
>
> The iOS build also pulls in `AuthenticationServices` and depends on `ApplicationCore`, because the
> account portal is presented through `ASWebAuthenticationSession`, which must be told which window to
> present over (`EOSIOSAuth.mm`, via the engine's `IOSAppDelegate`).

### 2.2) Steamworks
- Join the Steamworks partner program to access the SDK.
- Place contents so the following paths exist:
  - `ThirdParty/Steamworks/sdk/public`
  - `ThirdParty/Steamworks/sdk/redistributable_bin`
  - Windows: `ThirdParty/Steamworks/sdk/redistributable_bin/win64/steam_api64.dll`
  - Linux: `ThirdParty/Steamworks/sdk/redistributable_bin/linux64/libsteam_api.so`
  - Mac: `ThirdParty/Steamworks/sdk/redistributable_bin/osx/libsteam_api.dylib`

> `steam_api64.lib` is not used — Steamworks is bound at runtime too (see §3.7).

> Ensure your SDK versions match the expected filenames above. The build prints one line per backend
> (`[GamingServices] EOS: compiled in (...)` / `not available on <platform>, not compiled in`), which is
> the fastest way to confirm what a given target actually contains.

---

## 3) Selecting a Backend

Every SDK you vendor is compiled in and nothing is linked. Which backends actually run, and how they
are wired together, is a **profile declared in code** —
`Public/Native/GamingServiceProfile.h` — selected by the `GS_PROFILE` macro.

Config lives in code rather than ini on purpose: a build variant is then a one-token change that the
build system tracks, and a typo is a compile error instead of a runtime fallback to the wrong backend.
(`Game.ini` still holds credentials — just not the arrangement.)

### 3.1) Picking a profile

**The consuming game picks it, in its own target rules.** The plugin declares what the choices are and
nothing else needs editing inside it:

```csharp
// YourGame.Target.cs
ProjectDefinitions.Add("GS_PROFILE=SteamAuthIntoEpic");
```

Pin the same value in the editor target too — Standalone Game runs the editor binary with `-game`,
where the real platform service comes up, so a mismatch means editor testing exercises a different
arrangement than the packaged build.

A target that says nothing falls back to `DefaultProfile` in `GamingServices.Build.cs`, so the plugin
builds standalone. That is a fallback, not the switch.

Use `ProjectDefinitions`, **not** `GlobalDefinitions` — the latter alters the shared engine build
environment, and UBT rejects it on an installed engine ("has build products in common with
UnrealGame"). Setting it in the game *module*'s `Build.cs` does not work either: definitions there flow
downstream to modules that depend on the game, while the plugin is upstream, so it would silently keep
the fallback.

> There is deliberately no environment-variable switch. UBT caches its module-rules evaluation and does
> not treat the environment as a dependency, so an env-var override silently fails to take effect on an
> already-built tree.

### 3.2) The profiles

| Profile | Arrangement |
|---|---|
| `EpicOnly` | Pure EOS. Epic account login, EOS for everything. **Default.** |
| `SteamOnly` | Pure Steamworks. |
| `SteamAuthIntoEpic` | Steam signs the player in, EOS runs the session. |
| `EpicPreferredSteamFallback` | EOS where available, Steam otherwise. Two single-backend paths, never combined. |
| `Disabled` | Null backend; every capability honestly reports unsupported. |

A profile is plain data, so a new variant costs one declaration:

```cpp
inline constexpr FGamingServiceProfile SteamAuthIntoEpic
{
    .Name = TEXT("SteamAuthIntoEpic"),
    .Backends = { EGamingBackend::EpicOnlineServices },   // ordered; first that loads is primary
    .AuthBackend = EGamingBackend::Steamworks,            // omit for a plain single backend
    .bAllowAuthFallback = true,
};
```

A profile may also send one capability to a non-primary backend:

```cpp
    .CapabilityOverrides = { { EGamingCapability::User, EGamingBackend::Steamworks } },
```

Only when a profile actually wires backends together (an `AuthBackend` other than the primary, or a
`CapabilityOverrides` entry) does a composite service get built. Otherwise the factory hands back the
single backend unwrapped — no wrapper, no routing, no overhead.

### 3.3) Steam sign-in, EOS session

With `SteamAuthIntoEpic`, Steam mints a web-API session ticket, EOS Connect consumes it, and everything
after that — lobbies, P2P, stats, achievements, cloud saves — runs on EOS. The player never sees an
Epic login, and no Epic account is created: the EOS `ProductUserId` is the identity.

Requires a **Steam identity provider** configured in the EOS Dev Portal, with an identity string
matching the one the client sends (`epiconlineservices` by default, overridable with
`[GamingServices.Steamworks] WebApiIdentity`).

Identity is split deliberately: `GetUserId()` returns the EOS id every other capability keys on, while
display name and avatar come from Steam. The Steam persona is passed to EOS at login, so other players
see it in lobbies and on leaderboards. `bAllowAuthFallback` (default true) lets EOS's own login take
over when Steam cannot vouch — e.g. the game was launched outside the Steam client.

The mechanism is a pair of interfaces rather than special-cased code: a backend that can vouch for the
local user returns an `IExternalAuthProvider` (Steam), a backend that accepts someone else's credential
returns an `IExternalAuthConsumer` (EOS), and `FCompositeUser` pairs them into one login.

### 3.4) Testing one backend in isolation

A debug override runs an existing build against a single backend without rebuilding it, dropping any
cross-backend wiring so that backend is exercised alone:

```
TurtleRock.exe -GamingBackend=Steam
```

### 3.5) Excluding a backend from the build entirely

Rarely needed — an unused backend costs only its never-loaded staged library — but for a store build
that must not ship the other platform's binary at all:

```csharp
ProjectDefinitions.Add("GS_EXCLUDE_STEAM=1");
```

### 3.6) Credentials, and where to keep them

The arrangement lives in code; the credentials still come from ini, read from `[GamingServices.EOS]` /
`[GamingServices.Steamworks]` in `Game.ini`:

| Key | Secret? |
|---|---|
| `ProductName`, `ProductVersion`, `ProductId`, `SandboxId`, `DeploymentId` | No — product identifiers |
| `ClientId`, `ClientSecret`, `EOSEncryptionKey` | Treat as private to the team |
| `[GamingServices.Steamworks] AppId` | No |

`EOSEncryptionKey` is deliberately not named `EncryptionKey`: the bare name is on UE's staging
`IniKeyDenylist` (Engine `BaseGame.ini`), so it would be stripped from packaged config and a shipped
build would read it empty and fail to initialise.

Recommended home for the three private keys is **`<Project>/Config/GeneratedGame.ini`**, UE's
*ProjectGenerated* config layer. It loads after `DefaultGame.ini` and overrides it, is visible to
UnrealBuildTool's config hierarchy (so the Android UPL's build-time `ClientId` lookup still resolves),
and is staged into packaged builds. Epic reserves the name for files that are never checked in, so
gitignore `Config/Generated*.ini` and commit a `GeneratedGame.ini.template` with empty values beside it.
Add `+AllowedConfigFiles=<Project>/Config/GeneratedGame.ini` to `[Staging]` in `DefaultGame.ini` to tell
UAT the file is meant to ship (it stages either way, but warns about files it does not recognise).

Nothing in the plugin needs to know: `InitializePlatform` reads the merged hierarchy, and a missing key
is already a loud, named error that leaves the backend disabled rather than half-initialised. Per-field
`FEOSInitOptions` overrides still win over ini, which is how a test harness runs several instances
against different EOS clients.

> Be clear-eyed about what this buys. These are **client** credentials: they ship inside every packaged
> build and a player can extract them from the binary or the staged `Game.ini`. Keeping them out of the
> repo protects against leaks, forks and secret scanners, and makes rotation a one-file change — it does
> not hide them from players. The real boundary is the client policy attached to the `ClientId` in the
> EOS Dev Portal, so a client build must never carry a trusted-server client's credentials, and a Steam
> publisher Web API key must never be in a shipped build at all.

### 3.7) How the SDKs are bound

No import library and no delay-load entry is added for either SDK — the built binaries contain **zero**
import-table references to `steam_api64.dll` or `EOSSDK-Win64-Shipping.dll`. Instead:

- Every compiled-in backend's shared library is staged into one common folder:
  `<Project>/Binaries/ThirdParty/GamingServices/<Platform>/`
- EOS entry points are resolved into a symbol table at runtime
  (`Private/Native/EOS/EOSDynamicApi.*`), and call sites are redirected onto it by
  `EOSDynamicApiRedirect.h`, so capability code still reads as plain SDK usage.
- Steamworks needs only its handful of global C entry points bound
  (`Private/Native/Steam/SteamDynamicApi.cpp`); its C++ interfaces are pure-virtual and dispatch
  through vtables from the library itself.

A library that is missing or fails to resolve makes its backend report unavailable — the game still
starts, and `GetCapabilities()` honestly reports what is missing.

Game code never needs `#if` around connect/login parameters. Fill both `FEOSInitOptions` and
`FSteamworksInitOptions`; each backend reads its own section and ignores the rest.

---

## 4) Installing the Plugin into Your Project

1. Copy the `GamingServices` folder into your project's `Plugins/` directory
   (`YourProject/Plugins/GamingServices`), or add it as a submodule there.
2. Vendor the SDKs (§2). The plugin ships without them.
3. Add the credential and product keys to config (§3.6).
4. Pick a profile in your target rules (§3.1) — or accept the `EpicOnly` default.
5. To use P2P networking, register the net driver (§8). Skip this and the plugin still works; you just
   get platform services without P2P replication.

The module loads at `PostConfigInit` and builds the platform service itself, so there is nothing to call
at startup: by the time your `GameInstance` exists, the backend is initialised (or honestly reporting
unsupported).

---

## 5) Architecture

### 5.1) The capability model

`IGamingService` (`Native/IGamingService.h`) is the whole native surface: platform lifecycle plus one
accessor per capability. A backend overrides only what it implements; every other accessor keeps the
base's `nullptr`. So "not supported" is a plain null at the source:

```cpp
if (IMatchmakingService* MM = Service->GetMatchmaking())
{
    MM->CreateSession(Settings, Callback);
}
```

`GetCapabilities()` is computed from those accessors, so the flat `FGamingServiceCapabilities` snapshot
Blueprints see can never drift from what the code will actually do.

### 5.2) Ownership and lifecycle

| Piece | Responsibility |
|---|---|
| `FGamingServicesModule` | Owns the single `IGamingService`, builds it from the profile, initialises it in `StartupModule`, tears it down on engine pre-exit. Also owns the P2P socket subsystem registration. |
| `GamingServices::CreateGamingService()` | Factory. Returns a single backend, a preference-ordered fallback, or a composite — decided entirely by the profile. Never returns null (falls back to `FNullGamingService`). |
| `UGamingPlatformSubsystem` | Borrows the module's service, ticks it each frame, answers `GetCapabilities()` / `HasCapability()`, and resolves the service from a world context for the Blueprint libraries. Owns no operations and no events. |
| `FCompositeGamingService` | Only built for multi-backend arrangements. Drives every backend's lifecycle, routes capabilities per the profile, and replaces the user capability with `FCompositeUser` when an auth backend is configured. |

Exactly one backend initialises the SDK, and the service outlives the subsystem — which is why
net-driver sockets that die during GC ask the module for `GetP2PTransportOrNull()` rather than caching
a pointer.

### 5.3) What each backend implements

| Capability | EOS | Steamworks |
|---|---|---|
| Achievements | yes | yes |
| Entitlements | yes | yes |
| Leaderboards | yes | yes |
| Stats | yes | yes |
| Cloud storage | yes | yes |
| Remote settings | yes | yes |
| Matchmaking / lobbies | yes | yes |
| User identity | yes | yes |
| Avatars (sub-capability of User) | no — no native EOS avatar API, returns null | yes |
| Friends | only with an `EpicAccountId` | yes |
| P2P transport | yes | yes |
| External auth | consumer (accepts a credential) | provider (mints one) |
| Invite transport | — | yes (carries another backend's session id) |

Two entries deserve emphasis:

- **EOS Friends is null for a Connect-only sign-in.** Friends and Presence are Epic Account Services and
  need an `EpicAccountId`, which a Steam-authenticated user does not have. Reporting the capability as
  absent is the honest answer, and it makes a composite fall through to the identity backend's friend
  list instead.
- **Remote settings is not a platform feature.** `FRemoteSettingsStore` implements it generically over
  any `ICloudStorageService` as a single `game_settings.json` blob, so remote-settings support tracks
  cloud-storage support on every backend.

---

## 6) Blueprint Integration

Nothing to initialise and nothing to connect — the module already did it. Blueprints use two things:

**`UGamingPlatformSubsystem`** (Game Instance subsystem) for discovery only:
- `GetCapabilities()` → `FGamingServiceCapabilities` (one bool per capability)
- `HasCapability(EGamingCapability)`

**Per-capability libraries** in `Blueprint/Libraries/`, exposed as async-action nodes. Every node has the
built-in synchronous `then` pin plus a `Completed` pin carrying a result struct that derives from
`FGamingServiceResult` — so you branch on `Result.bSuccess` rather than wiring separate success/failure
execs.

| Category | Async nodes | Pure / immediate |
|---|---|---|
| User | `Login` | `IsLoggedIn`, `NeedsLogin`, `GetUserId`, `GetDisplayName`, `GetAvatar`, `GetAvatarByUserId` |
| Achievements | `UnlockAchievement`, `QueryAchievements` | — |
| Leaderboards | `WriteLeaderboardScore`, `QueryLeaderboardPage`, `QueryLeaderboardUserRank` | — |
| Stats | `IngestStat`, `QueryStat` | — |
| Cloud storage | `WriteFile`, `ReadFile`, `DeleteFile`, `ListFiles` | — |
| Remote settings | `SetRemoteSetting`, `GetRemoteSetting`, `DeleteRemoteSetting`, `ListRemoteSettings` | — |
| Entitlements | `ListEntitlements`, `HasEntitlement` | — |
| Friends | `QueryFriends`, `SendFriendInvite` | `IsFriendsAvailable`, `GetCachedFriends`, `GetFriendCount`, `GetFriendAt` |
| Matchmaking | `CreateSession`, `FindSessions`, `JoinSession`, `LeaveSession`, `DestroySession`, `UpdateSession`, `LockLobby`, `UnlockLobby`, `GetCurrentSession`, `ShowInviteFriendsDialog` | `GetSessionConnectionString` |

Login options mirror the native struct: set `FGamingServiceLoginParams.EOS.Method` to `PersistentAuth`,
`AccountPortal`, `DeviceCode` or `Developer` (the last needs `DeveloperHost` and
`DeveloperCredentialName`). Steam takes no login options — the running client *is* the credential.

> Notification **sinks** (session members joining/leaving, invites arriving, avatars finishing download)
> are `TFunction` members on the native interfaces, so they are bound from C++. Surface them to
> Blueprints from your own game subsystem — that keeps ownership of "who listens" in game code, where a
> single owner can re-broadcast on a gameplay message or a dynamic delegate.

---

## 7) C++ Integration

Get the service, ask for a capability, use it if you got one:

```cpp
#include "Blueprint/GamingPlatformSubsystem.h"
#include "Native/IGamingService.h"
#include "Native/Interfaces/IUserService.h"
#include "Native/Interfaces/IMatchmakingService.h"
#include "Native/Interfaces/IAchievementsService.h"

void UMyGameSubsystem::Start()
{
    UGamingPlatformSubsystem* Platform = GetGameInstance()->GetSubsystem<UGamingPlatformSubsystem>();
    IGamingService* Service = Platform ? Platform->GetService() : nullptr;
    if (!Service)
    {
        return; // no platform on this build/machine — run offline
    }

    // Identity: log in if the platform needs it (Steam is already signed in; EOS may not be).
    if (IUserService* User = Service->GetUser())
    {
        if (User->NeedsLogin())
        {
            FGamingServiceLoginParams Params;
            Params.EOS.Method = EEOSLoginMethod::PersistentAuth;
            User->Login(Params, [](const FGamingServiceResult& Result)
            {
                // Result.bSuccess — see §12 on why there is no error detail here yet
            });
        }
    }

    // Sessions: bind the sinks, then drive the operations.
    if (IMatchmakingService* MM = Service->GetMatchmaking())
    {
        MM->OnSessionUserJoined = [](const FSessionMemberInfo& Member) { /* ... */ };
        MM->OnSessionUserLeft   = [](const FSessionMemberInfo& Member) { /* ... */ };
        MM->OnSessionEnded      = [](const FGamingServiceResult& R)    { /* ... */ };

        FSessionSettings Settings;
        MM->CreateSession(Settings, [](const FSessionCreateResult& R) { /* ... */ });
    }

    // Anything the backend does not implement is simply null — no error paths to handle.
    if (IAchievementsService* Achievements = Service->GetAchievements())
    {
        Achievements->UnlockAchievement(TEXT("ACH_WIN_FIRST_LEVEL"), [](const FGamingServiceResult& R) {});
    }
}
```

Notes:
- Do **not** call `InitializePlatform` / `DestroyPlatform` from game code; the module owns both.
- `UGamingPlatformSubsystem` ticks the service, so SDK callbacks are pumped without your help.
- Member events carry ids, not names: both backends fire `FSessionMemberInfo` with `DisplayName` set to
  the id. `IUserService::ResolveDisplayName` turns one into a real name asynchronously, always calling
  back exactly once, and falls back to the id string so the value is never empty.
- The sinks are single-assignment `TFunction`s, not multicast delegates: one owner per sink. Give that
  owner the job of re-broadcasting to the rest of the game.

---

## 8) P2P Networking (net driver)

`UMinderaNetDriver` runs Unreal replication over a platform relay. It is a thin `UIpNetDriver` that
swaps in a P2P socket subsystem and lets the engine do the rest — connections, the connectionless
handshake and channels all behave as they do over UDP.

The SDK-free `IP2PTransport` is what makes it backend-agnostic. It is deliberately **connectionless**,
matching UE's model: send a datagram to a peer id on a virtual-port channel, receive datagrams tagged
with their source peer id. Each backend implements it in one `.cpp` that includes only its own SDK —
EOS is natively connectionless (`EOS_P2P_SendPacket` / `ReceivePacket`), while the Steam implementation
keeps the peer↔connection map and auto-accept dance internally, so no netdriver header ever sees a
platform type.

Peers are addressed by an opaque id string (a `SteamID64` or an EOS `ProductUserId`), and connect URLs
are tagged with the transport's own prefix (`steam.` / `eos.`).

Register it in `DefaultEngine.ini`:

```ini
[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/GamingServices.MinderaNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[/Script/UnrealEd.UnrealEdEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/GamingServices.MinderaNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[/Script/GamingServices.MinderaNetDriver]
NetConnectionClassName="/Script/GamingServices.MinderaNetConnection"
```

**Passthrough is automatic and URL-driven.** A non-P2P host (LAN, PIE, a raw IP) falls back to the
platform socket subsystem, so there is no editor special-casing: the same build listens on a relay in
shipping and on UDP in PIE.

Two things to know when adopting it:

- **Pin the platform online subsystem.** Identity here comes from GamingServices, and neither the driver
  nor the connection touches `FUniqueNetIdRepl` — but the login handshake still carries whatever id the
  *default* platform OSS mints, and `AGameModeBase::PreLogin` rejects a client whose id type differs from
  the server's (`incompatible_unique_net_id`). Left unset, each platform picks a different default
  (iOS → `IOS`, Android → `GooglePlay`, which falls back to `NULL`), and a cross-platform join is refused.
- **Sockets can outlive teardown.** GC destroys them after `OnEnginePreExit`, so anything holding a
  transport must fetch it through `FGamingServicesModule::GetP2PTransportOrNull()` rather than caching.

---

## 9) Invites

Invites are social, and the social graph does not always live on the backend running the session — with
`SteamAuthIntoEpic`, friends are on Steam while the joinable thing is an EOS lobby id. So invites are
two interfaces that meet in the middle:

- `IInviteTransport` (Steam) publishes an **opaque payload** (`SetJoinInfo`), opens the platform's own
  friend picker, and reports acceptances through `OnJoinRequested`. It never interprets the payload.
- `IMatchmakingService::JoinLobbyById` turns that payload back into a joined lobby, with no search
  bucket, presence or friends dependency. The same call backs an out-of-band "join code".

Accepting an invite while the game is closed launches it, so a payload seen at startup is queued and
delivered when the game calls `FlushPendingJoin()` — binding the sink is not enough, because the sink is
bound while the platform layer is still coming up, well before the game can act on a join.

Pre-acceptance invites differ per platform, and that difference is exposed rather than hidden:
`PlatformOwnsInviteUI()` is true when an overlay the game cannot intercept owns the decision (Steam). In
that case only `OnLobbyInviteAccepted` ever fires and drawing your own toast would duplicate the
overlay; when it is false, nothing shows an invite unless the game does, and `OnLobbyInviteReceived`
plus `RejectInvite` are the accept/decline pair. It is runtime state, not a build property: a build that
normally defers to an overlay must still draw its own UI when that platform turns out to be unavailable.

`QueryPendingInvites` covers what the live sink cannot — invites that arrived before launch or during
sign-in. Poll it once after login.

---

## 10) Shipping Binaries

The build rules add the runtime dependency for each compiled-in backend and copy its library into the
common folder at build time, so editor and packaged runs load the same file:

```
<Project>/Binaries/ThirdParty/GamingServices/<Platform>/
```

Android is the exception: gradle packages `libEOSSDK.so` from the AAR, so nothing is staged there.

Confirm the `[GamingServices]` build lines name the libraries you expect, and that credential config
survives cooking (§3.6 — a key named `EncryptionKey` will not).

---

## 11) Troubleshooting

| Symptom | Likely cause |
|---|---|
| `Missing required EOS settings: …` | The named keys are absent from the merged ini. Check `Config/GeneratedGame.ini` exists and that the key is `EOSEncryptionKey`, not `EncryptionKey`. |
| Every capability reports unsupported | No backend available: SDK not vendored for this platform, library missing from the common folder, or the profile is `Disabled`. The startup log says which. |
| Build says `not available on <platform>, not compiled in` | Expected on platforms with no vendored binary (e.g. Steam on Android or iOS) — otherwise check the `ThirdParty` layout and filenames, case-sensitively. On iOS it also means `SDK-IOS/Bin/EOSSDK.framework/Headers/` is missing — that path *is* the include path there (§2.1). |
| Build says `ERROR: EOS iOS framework missing` | `SDK-IOS/Bin/EOSSDK.embeddedframework.zip` is absent. It is not part of the download — build it (§2.1). |
| Login fails on a Steam-into-EOS build | No Steam identity provider in the EOS Dev Portal, or a `WebApiIdentity` mismatch, or the game was launched outside the Steam client (watch for the `bAllowAuthFallback` path taking over). |
| `GetFriends()` returns null on EOS | Connect-only sign-in has no `EpicAccountId`. Expected — use the identity backend's friend list. |
| Avatars are null on EOS | No native EOS avatar API. Route the User capability to Steam via a `CapabilityOverrides` entry, or draw a fallback. |
| Client refused with `incompatible_unique_net_id` | Platform OSS defaults differ across platforms — pin one explicitly (§8). |
| Blueprint nodes missing | Plugin not enabled, or the project was not rebuilt after adding it. |
| A profile name does not compile | That is the design: `GS_PROFILE` resolves against the declarations in `GamingServiceProfile.h`, so a typo fails the build naming the culprit. |

---

## 12) Roadmap

- Google Play Services as a third backend
- Unified achievement / stat naming, so ids are not backend-specific at the call site
- Error reporting. `FGamingServiceResult` is `bSuccess` and nothing else: no code, no message. Failures
  are distinguishable only in the log, so a caller cannot tell "declined" from "offline" from "not
  entitled". A standardised error enum plus platform detail is the biggest gap in the API.
- Automated tests around the capability layer (a commandlet harness drives EOS in isolation today)
- Optional backend-specific extension interfaces (SteamUGC, Workshop, Inventory)

---

## 13) License

This plugin depends on third-party SDKs subject to their own licenses. Consult Epic and Valve for terms.

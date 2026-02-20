## UnrealGamingServices Plugin

Cross-platform abstraction over common gaming platform services. Supports Epic Online Services (EOS) and Steamworks backends. Exposes a unified C++ interface and Blueprint-friendly `UGamingServicesSubsystem` for achievements, leaderboards, and stats.

### Features
- Unified API for multiple backends (EOS, Steamworks)
- Blueprint callable subsystem (`UGamingServicesSubsystem`)
- Achievements
- Leaderboards
- Stats
- Cloud storage
- Cloud config
- Sessions/Matchmaking

### Planned features
- Google Play Services API support as a backend
- Per-backend capability querying: Move code backend code into interfaces that the user can query as not all backends support the same features
- (Possibly) Unified achievements system: Currently achievement names have to be called with backend specific name, but it would be better to accept the names/requirements for each backend or something of the sort
- Look into testing
- Entitlements
- Error standardization
- Optional extension interfaces (backend-specific advanced features): IGamingServicesSteamExtension (SteamUGC, SteamWorkshop, SteamInventory)

---

## 1) Requirements
- Unreal Engine 5.x
- Platform SDKs (place under this plugin's `ThirdParty` folder):
  - Epic Online Services SDK
  - Steamworks SDK

> Note: The plugin ships without SDKs. You must download them and place them locally as described below.

---

## 2) SDK Download and Placement
Download the SDKs and place them inside the plugin `ThirdParty` directory with the following structure:

```
GamingServices/
  ThirdParty/
    EOS/
      SDK/
        Include/
        Lib/
        Bin/
    Steamworks/
      sdk/
        public/
        redistributable_bin/
        win64/            # present in Steam package (contains steam_api64.dll)
```

### 2.1) Epic Online Services (EOS)
- Download: `https://dev.epicgames.com/docs` (navigate to Epic Online Services SDK)
- Place contents so the include/lib/bin paths match:
  - `ThirdParty/EOS/SDK/Include`
  - `ThirdParty/EOS/SDK/Lib`
  - `ThirdParty/EOS/SDK/Bin`

The build script expects platform binaries at:
- Windows: `EOSSDK-Win64-Shipping.lib` and `EOSSDK-Win64-Shipping.dll`
- Linux: `libEOSSDK-Linux-Shipping.so`
- LinuxArm64: `libEOSSDK-LinuxArm64-Shipping.so`
- Mac: `libEOSSDK-Mac-Shipping.dylib`

### 2.2) Steamworks
- Join the Steamworks partner program to access the SDK.
- Place contents so the following paths exist:
  - `ThirdParty/Steamworks/sdk/public`
  - `ThirdParty/Steamworks/sdk/redistributable_bin`
  - Windows 64-bit redistributables:
    - `ThirdParty/Steamworks/sdk/win64/steam_api64.dll`
    - `ThirdParty/Steamworks/sdk/redistributable_bin/win64/steam_api64.lib`
  - Linux: `ThirdParty/Steamworks/sdk/redistributable_bin/linux64/libsteam_api.so`
  - Mac: `ThirdParty/Steamworks/sdk/redistributable_bin/osx/libsteam_api.dylib`

> Ensure your SDK versions match the expected filenames above.

---

## 3) Selecting a Backend (EOS vs Steamworks)
In `Source/GamingServices/GamingServices.Build.cs`, switch the backend define:

```23:39:Source/GamingServices/GamingServices.Build.cs
        // Switch backend here
        const EServiceBackends backend = EServiceBackends.EpicOnlineServices;

        switch (backend)
        {
            case EServiceBackends.EpicOnlineServices:
                PublicDefinitions.Add("USE_EOS");
                AddEOS(Target);
                break;
            case EServiceBackends.Steamworks:
                AddSteamworks(Target);
                PublicDefinitions.Add("USE_STEAMWORKS");
                break;
        }
```

Set `backend` to `EpicOnlineServices` or `Steamworks` and rebuild.

Notes:
- This selection is a build-time choice that controls which backend implementation is compiled in and instantiated by the subsystem.
- Your game code does not need `#if USE_EOS` / `#if USE_STEAMWORKS` around connect/login parameters. You can populate both sets of parameters; the active backend will read the relevant section and ignore the rest.

---

## 4) Installing the Plugin into Your Project
1. Copy the `GamingServices` folder into your project's `Plugins/` directory:
   - `YourProject/Plugins/GamingServices`
2. Open the project in Unreal Editor. The plugin should appear as `GamingServices` and load at `Runtime`.
3. Ensure the SDK folders are present as described above before compiling.

> Alternatively, you can keep it in the Engine `Plugins/` directory if you prefer engine-wide availability.

---

## 5) Blueprint Integration
The plugin exposes a `UGamingServicesSubsystem` with Blueprint-callable functions and events. Add and use it via the Game Instance.

### 5.1) Connect
Use `Connect` once at startup (e.g., in Game Instance BeginPlay). For EOS, fill the `FEOSInitOptions` fields.

Example (Blueprint):
- Call `Connect` (Subsystem: `GamingServicesSubsystem`) and set:
  - `EOS.ProductName`, `EOS.ProductVersion`, `EOS.ProductId`, `EOS.SandboxId`, `EOS.DeploymentId`, `EOS.ClientId`, `EOS.ClientSecret`.

### 5.2) Login
- Call `Login` on the subsystem and set `FEOSLoginOptions.Method` to one of:
  - `PersistentAuth`, `AccountPortal`, `DeviceCode`, `Developer`
- For `Developer` method, set `DeveloperHost` and `DeveloperCredentialName`.

### 5.3) Achievements
- `UnlockAchievement(AchievementId)`
- `QueryAchievements()`
- Bind to `OnAchievementUnlocked` and `OnAchievementsQueried` for results.

### 5.4) Leaderboards
- `WriteLeaderboardScore(LeaderboardId, Score)`
- `QueryLeaderboardPage(LeaderboardId, Limit, ContinuationToken)`
- Bind to `OnLeaderboardScoreWritten` and `OnLeaderboardQueried`.

### 5.5) Stats
- `IngestStat(StatName, Amount)`
- `QueryStat(StatName)`
- Bind to `OnStatProgressed` and `OnStatQueried`.

### 5.6) Auth Helpers
- `IsConnected()`, `IsLoggedIn()`, `NeedsLogin()`

---

## 6) C++ Integration (subsystem-managed, call native API)
Recommended flow: initialize via `UGamingServicesSubsystem` for lifetime and ticking, then fetch the underlying `IGamingService` and call the native API from C++.

Caller is responsible for providing connection parameters. It's safe to fill both EOS and Steamworks fields without macros; the compiled backend will use only what it needs.
- EOS requires: `ProductName`, `ProductVersion`, `ProductId`, `SandboxId`, `DeploymentId`, `ClientId`, `ClientSecret`.
- Steamworks: `FSteamworksInitOptions` is currently empty.

```cpp
#include "GamingServicesSubsystem.h"
#include "GamingServiceTypes.h"

void UseServiceFromSubsystem(UGameInstance* GI)
{
    if (GI)
    {
        if (UGamingServicesSubsystem* Subsystem = GI->GetSubsystem<UGamingServicesSubsystem>())
        {
            // Fill BOTH EOS and Steamworks sections; the active backend will read its own
            FGamingServiceConnectParams ConnectParams;
            ConnectParams.EOS.ProductName = TEXT("YourProduct");
            ConnectParams.EOS.ProductVersion = TEXT("1.0");
            ConnectParams.EOS.ProductId = TEXT("YOUR_PRODUCT_ID");
            ConnectParams.EOS.SandboxId = TEXT("YOUR_SANDBOX_ID");
            ConnectParams.EOS.DeploymentId = TEXT("YOUR_DEPLOYMENT_ID");
            ConnectParams.EOS.ClientId = TEXT("YOUR_CLIENT_ID");
            ConnectParams.EOS.ClientSecret = TEXT("YOUR_CLIENT_SECRET");
            // ConnectParams.Steamworks is currently empty but can be extended in the future

            // You can connect via the subsystem OR directly via the underlying service:
            // Option A: Subsystem connects (delegates to service)
            // if (!Subsystem->Connect(ConnectParams)) { return; }

            // Option B: Connect on the service itself
            IGamingService& Service = Subsystem->GetService();
            if (!Service.Connect(ConnectParams))
            {
                return;
            }

            FGamingServiceLoginParams LoginParams;
            Service.Login(LoginParams, [](const FGamingServiceResult& Result)
            {
                if (!Result.bSuccess)
                {
                    // handle login failure
                    return;
                }
                // proceed after successful login
            });

            // Achievements
            Service.UnlockAchievement(TEXT("ACH_WIN_FIRST_LEVEL"), [](const FGamingServiceResult& R)
            {
                // handle unlock result
            });

            // Leaderboards
            Service.WriteLeaderboardScore(TEXT("HIGHSCORE"), 12345, [](const FGamingServiceResult& R){});
            Service.QueryLeaderboardPage(TEXT("HIGHSCORE"), 50, 0, [](const FLeaderboardResult& R){});

            // Stats
            Service.IngestStat(TEXT("EnemiesDefeated"), 10, [](const FGamingServiceResult& R)
            {
                // handle ingest result
            });
            Service.QueryStat(TEXT("EnemiesDefeated"), [](const FStatQueryResult& R)
            {
                // R.bSuccess, R.StatName, R.Value
            });
        }
    }
}
```

Notes:
- The subsystem ticks the service automatically; you do not need to call `Tick()` yourself.
- Use `Service.IsInitialized()`, `Service.IsLoggedIn()`, and `Service.NeedsLogin()` for C++ state checks.

---

## 7) Shipping Binaries
The build rules automatically add runtime dependencies for SDK binaries per platform. Ensure the expected files exist under `ThirdParty` so they are staged in packaged builds.

---

## 8) Troubleshooting
- If the plugin compiles but fails to initialize:
  - Verify SDK folder layout and filenames.
  - Confirm `GamingServices.Build.cs` backend selection matches the SDK you provided.
  - Check your EOS credentials and deployment identifiers.
- If Blueprint nodes are missing:
  - Ensure the plugin is enabled and the project was rebuilt.
- If packaged build fails to load SDK DLLs:
  - Double-check `ThirdParty` binary filenames and case sensitivity on non-Windows platforms.

---

## 9) License
This plugin depends on third-party SDKs subject to their own licenses. Consult EOS and Valve for terms.



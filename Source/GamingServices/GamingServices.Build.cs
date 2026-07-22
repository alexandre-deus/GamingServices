using System;
using System.IO;
using UnrealBuildTool;

public class GamingServices : ModuleRules
{
    private string PluginRoot => Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
    private string ProjectRoot => Path.GetFullPath(Path.Combine(PluginRoot, "..", ".."));

    private string BinariesDir(ReadOnlyTargetRules Target) =>
        Path.Combine(ProjectRoot, "Binaries", Target.Platform.ToString());

    private void ForceCopy(string source, string destDir, string filename)
    {
        if (!File.Exists(source))
        {
            Console.WriteLine($"[GamingServices]   ERROR: source file does not exist: {source}");
            return;
        }

        Directory.CreateDirectory(destDir);
        string dest = Path.Combine(destDir, filename);

        try
        {
            File.Copy(source, dest, overwrite: true);
            Console.WriteLine($"[GamingServices]   OK: copied {filename} -> {destDir}");
        }
        catch (IOException)
        {
            Console.WriteLine($"[GamingServices]   SKIPPED: {filename} is in use (already loaded), this is fine");
        }
    }

    public bool IsEOSAvailable()
    {
        string EOSRoot = Path.Combine(PluginRoot, "ThirdParty", "EOS", "SDK");
        bool hasInclude = Directory.Exists(Path.Combine(EOSRoot, "Include"));
        bool hasLib     = Directory.Exists(Path.Combine(EOSRoot, "Lib"));
        bool hasBin     = Directory.Exists(Path.Combine(EOSRoot, "Bin"));

        Console.WriteLine($"[GamingServices] IsEOSAvailable check:");
        Console.WriteLine($"[GamingServices]   Root:    {EOSRoot}");
        Console.WriteLine($"[GamingServices]   Include: {hasInclude}");
        Console.WriteLine($"[GamingServices]   Lib:     {hasLib}");
        Console.WriteLine($"[GamingServices]   Bin:     {hasBin}");

        return hasInclude && hasLib && hasBin;
    }

    // EOS binaries are only vendored for desktop platforms (see AddEOS). On any other platform
    // (e.g. Android) selecting EOS would compile the EOS interface code but leave every EOS_* symbol
    // unlinked, so the backend must fall back to Null there.
    public bool IsEOSSupportedOnPlatform(ReadOnlyTargetRules Target)
    {
        return Target.Platform == UnrealTargetPlatform.Win64
            || Target.Platform == UnrealTargetPlatform.Linux
            || Target.Platform == UnrealTargetPlatform.LinuxArm64
            || Target.Platform == UnrealTargetPlatform.Mac
            || Target.Platform == UnrealTargetPlatform.Android;
    }

    public void AddEOS(ReadOnlyTargetRules Target)
    {
        Console.WriteLine($"[GamingServices] AddEOS for platform: {Target.Platform}");

        bool bAndroid = Target.Platform == UnrealTargetPlatform.Android;

        // Android ships as a separate, self-contained SDK (AAR + per-ABI .so, a newer EOS version)
        // vendored alongside the desktop SDK. Desktop uses ThirdParty/EOS/SDK; Android uses
        // ThirdParty/EOS/SDK-Android, each with its own headers.
        string EOSRoot    = Path.Combine(PluginRoot, "ThirdParty", "EOS", bAndroid ? "SDK-Android" : "SDK");
        string EOSInclude = Path.Combine(EOSRoot, "Include");
        string EOSLibDir  = Path.Combine(EOSRoot, "Lib");
        string EOSBinDir  = Path.Combine(EOSRoot, "Bin");
        string outDir     = BinariesDir(Target);

        Console.WriteLine($"[GamingServices]   EOSRoot:   {EOSRoot}");
        Console.WriteLine($"[GamingServices]   OutputDir: {outDir}");

        PrivateIncludePaths.Add(EOSInclude);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string lib = Path.Combine(EOSLibDir, "EOSSDK-Win64-Shipping.lib");
            string dll = Path.Combine(EOSBinDir, "EOSSDK-Win64-Shipping.dll");

            Console.WriteLine($"[GamingServices]   Lib exists: {File.Exists(lib)} -> {lib}");
            Console.WriteLine($"[GamingServices]   DLL exists: {File.Exists(dll)} -> {dll}");

            PublicAdditionalLibraries.Add(lib);
            PublicDelayLoadDLLs.Add("EOSSDK-Win64-Shipping.dll");
            // Stage relative to the project, not $(TargetOutputDir): for Editor targets the output
            // dir is the ENGINE binaries folder, where the engine's own bundled EOSSDK module also
            // stages this dll, and the conflicting sources fail the build.
            RuntimeDependencies.Add("$(ProjectDir)/Binaries/Win64/EOSSDK-Win64-Shipping.dll", dll);

            ForceCopy(dll, outDir, "EOSSDK-Win64-Shipping.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string so = Path.Combine(EOSBinDir, "libEOSSDK-Linux-Shipping.so");
            Console.WriteLine($"[GamingServices]   SO exists: {File.Exists(so)} -> {so}");
            RuntimeDependencies.Add("$(TargetOutputDir)/libEOSSDK-Linux-Shipping.so", so);
            ForceCopy(so, outDir, "libEOSSDK-Linux-Shipping.so");
        }
        else if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
        {
            string so = Path.Combine(EOSBinDir, "libEOSSDK-LinuxArm64-Shipping.so");
            Console.WriteLine($"[GamingServices]   SO exists: {File.Exists(so)} -> {so}");
            RuntimeDependencies.Add("$(TargetOutputDir)/libEOSSDK-LinuxArm64-Shipping.so", so);
            ForceCopy(so, outDir, "libEOSSDK-LinuxArm64-Shipping.so");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string dylib = Path.Combine(EOSBinDir, "libEOSSDK-Mac-Shipping.dylib");
            Console.WriteLine($"[GamingServices]   Dylib exists: {File.Exists(dylib)} -> {dylib}");
            RuntimeDependencies.Add("$(TargetOutputDir)/libEOSSDK-Mac-Shipping.dylib", dylib);
            ForceCopy(dylib, outDir, "libEOSSDK-Mac-Shipping.dylib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // One libEOSSDK.so per ABI (arm64-v8a, x86_64 — the vendored AAR has no armeabi-v7a).
            // These are linked only for symbol resolution; the runtime .so plus the Java/JNI bridge
            // (com.epicgames.mobile.eossdk.EOSSDK) are packaged into the APK from the AAR by
            // EOS_Android_UPL.xml, which also calls EOSSDK.init() and registers the auth-handler activity.
            string StaticStdc = Path.Combine(EOSBinDir, "Android", "static-stdc++");
            foreach (string abi in new[] { "arm64-v8a", "x86_64" })
            {
                string so = Path.Combine(StaticStdc, "libs", abi, "libEOSSDK.so");
                Console.WriteLine($"[GamingServices]   SO exists ({abi}): {File.Exists(so)} -> {so}");
                PublicAdditionalLibraries.Add(so);
            }

            string uplPath = Path.Combine(PluginRoot, "EOS_Android_UPL.xml");
            Console.WriteLine($"[GamingServices]   UPL exists: {File.Exists(uplPath)} -> {uplPath}");
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", uplPath);
        }
        else
        {
            Console.WriteLine($"[GamingServices]   WARNING: unsupported platform {Target.Platform}, skipping EOS binaries");
        }
    }

    public bool IsSteamworksAvailable()
    {
        string SteamRoot = Path.Combine(PluginRoot, "ThirdParty", "Steamworks", "sdk");
        bool hasPublic  = Directory.Exists(Path.Combine(SteamRoot, "public"));
        bool hasBinRoot = Directory.Exists(Path.Combine(SteamRoot, "redistributable_bin"));

        Console.WriteLine($"[GamingServices] IsSteamworksAvailable check:");
        Console.WriteLine($"[GamingServices]   Root:              {SteamRoot}");
        Console.WriteLine($"[GamingServices]   public/:           {hasPublic}");
        Console.WriteLine($"[GamingServices]   redistributable_bin/: {hasBinRoot}");

        return hasPublic && hasBinRoot;
    }

    public void AddSteamworks(ReadOnlyTargetRules Target)
    {
        Console.WriteLine($"[GamingServices] AddSteamworks for platform: {Target.Platform}");

        string SteamRoot    = Path.Combine(PluginRoot, "ThirdParty", "Steamworks", "sdk");
        string SteamInclude = Path.Combine(SteamRoot, "public");
        string SteamBinRoot = Path.Combine(SteamRoot, "redistributable_bin");
        string outDir       = BinariesDir(Target);

        Console.WriteLine($"[GamingServices]   SteamRoot:  {SteamRoot}");
        Console.WriteLine($"[GamingServices]   OutputDir:  {outDir}");

        PrivateIncludePaths.Add(SteamInclude);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string lib   = Path.Combine(SteamBinRoot, "win64", "steam_api64.lib");
            string dll   = Path.Combine(SteamBinRoot, "win64", "steam_api64.dll");
            string appId = Path.Combine(PluginRoot, "steam_appid.txt");

            Console.WriteLine($"[GamingServices]   Lib exists:   {File.Exists(lib)} -> {lib}");
            Console.WriteLine($"[GamingServices]   DLL exists:   {File.Exists(dll)} -> {dll}");
            Console.WriteLine($"[GamingServices]   AppId exists: {File.Exists(appId)} -> {appId}");

            PublicAdditionalLibraries.Add(lib);
            PublicDelayLoadDLLs.Add("steam_api64.dll");
            RuntimeDependencies.Add("$(TargetOutputDir)/steam_api64.dll", dll);

            ForceCopy(dll,   outDir, "steam_api64.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string so = Path.Combine(SteamBinRoot, "linux64", "libsteam_api.so");
            Console.WriteLine($"[GamingServices]   SO exists: {File.Exists(so)} -> {so}");
            RuntimeDependencies.Add("$(TargetOutputDir)/libsteam_api.so", so);
            ForceCopy(so, outDir, "libsteam_api.so");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string dylib = Path.Combine(SteamBinRoot, "osx", "libsteam_api.dylib");
            Console.WriteLine($"[GamingServices]   Dylib exists: {File.Exists(dylib)} -> {dylib}");
            RuntimeDependencies.Add("$(TargetOutputDir)/libsteam_api.dylib", dylib);
            ForceCopy(dylib, outDir, "libsteam_api.dylib");
        }
        else
        {
            Console.WriteLine($"[GamingServices]   WARNING: unsupported platform {Target.Platform}, skipping Steamworks binaries");
        }
    }

    public enum EServiceBackends
    {
        Steamworks,
        EpicOnlineServices,
        Null,
    }

    public GamingServices(ReadOnlyTargetRules Target) : base(Target)
    {
        Console.WriteLine($"[GamingServices] ============ GamingServices.Build.cs ============");
        Console.WriteLine($"[GamingServices] ModuleDirectory: {ModuleDirectory}");
        Console.WriteLine($"[GamingServices] PluginRoot:      {PluginRoot}");
        Console.WriteLine($"[GamingServices] ProjectRoot:     {ProjectRoot}");
        Console.WriteLine($"[GamingServices] BinariesDir:     {BinariesDir(Target)}");
        Console.WriteLine($"[GamingServices] Platform:        {Target.Platform}");
        Console.WriteLine($"[GamingServices] TargetType:      {Target.Type}");
        Console.WriteLine($"[GamingServices] TargetName:      {Target.Name}");
        Console.WriteLine($"[GamingServices] ==================================================");

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Per-backend private roots so internal headers resolve by name across the Steam/EOS subfolders
        // (e.g. "SteamPlatformCore.h", "SteamCallResultManager.h", "Interfaces/SteamMatchmaking.h").
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "Native", "Steam"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "Native", "EOS"));

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "Projects",
            "Json",
            "JsonUtilities",
            "NetCore",
            "Sockets",
            "PacketHandler",
            "OnlineSubsystemUtils",
        });

        // Default backend for shipping builds. The GAMINGSERVICES_BACKEND environment variable
        // overrides it at build time (used by the isolated test harness to select EOS without
        // touching source): Steamworks | EpicOnlineServices | Null.
        EServiceBackends backend = EServiceBackends.EpicOnlineServices;
        string backendEnv = Environment.GetEnvironmentVariable("GAMINGSERVICES_BACKEND");
        if (!string.IsNullOrEmpty(backendEnv) && Enum.TryParse(backendEnv, true, out EServiceBackends parsedBackend))
        {
            backend = parsedBackend;
            Console.WriteLine($"[GamingServices] Backend overridden by GAMINGSERVICES_BACKEND={backendEnv}");
        }

        Console.WriteLine($"[GamingServices] Selected backend: {backend}");

        bool bServiceConfigured = false;

        switch (backend)
        {
            case EServiceBackends.EpicOnlineServices:
                if (!IsEOSSupportedOnPlatform(Target))
                {
                    Console.WriteLine($"[GamingServices] EOS not supported on {Target.Platform}, falling back to null service.");
                }
                else if (IsEOSAvailable())
                {
                    Console.WriteLine($"[GamingServices] EOS SDK found, configuring...");
                    PublicDefinitions.Add("USE_EOS=1");
                    AddEOS(Target);
                    bServiceConfigured = true;
                }
                else
                {
                    Console.WriteLine($"[GamingServices] EOS SDK not found, skipping.");
                }
                break;

            case EServiceBackends.Steamworks:
                if (IsSteamworksAvailable())
                {
                    Console.WriteLine($"[GamingServices] Steamworks SDK found, configuring...");
                    PublicDefinitions.Add("USE_STEAMWORKS=1");
                    PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
                    AddSteamworks(Target);
                    bServiceConfigured = true;
                }
                else
                {
                    Console.WriteLine($"[GamingServices] Steamworks SDK not found, skipping.");
                }
                break;
        }

        if (!bServiceConfigured)
        {
            Console.WriteLine($"[GamingServices] WARNING: {backend} SDK not available. Falling back to null service.");
        }

        Console.WriteLine($"[GamingServices] Build.cs evaluation complete. Configured: {bServiceConfigured}");
    }
}
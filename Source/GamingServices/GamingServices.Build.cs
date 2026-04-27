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

    public void AddEOS(ReadOnlyTargetRules Target)
    {
        Console.WriteLine($"[GamingServices] AddEOS for platform: {Target.Platform}");

        string EOSRoot    = Path.Combine(PluginRoot, "ThirdParty", "EOS", "SDK");
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
            RuntimeDependencies.Add("$(TargetOutputDir)/EOSSDK-Win64-Shipping.dll", dll);

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

        const EServiceBackends backend = EServiceBackends.Steamworks;

        Console.WriteLine($"[GamingServices] Selected backend: {backend}");

        bool bServiceConfigured = false;

        switch (backend)
        {
            case EServiceBackends.EpicOnlineServices:
                if (IsEOSAvailable())
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
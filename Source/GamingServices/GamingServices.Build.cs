using System;
using System.IO;
using UnrealBuildTool;

public class GamingServices : ModuleRules
{
    private string PluginRoot => Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
    private string ThirdPartyRoot => Path.Combine(PluginRoot, "ThirdParty");

    public bool IsEOSAvailable()
    {
        var EOSRoot = Path.Combine(ThirdPartyRoot, "EOS", "SDK");
        var EOSInclude = Path.Combine(EOSRoot, "Include");
        var EOSLibDir = Path.Combine(EOSRoot, "Lib");
        var EOSBinDir = Path.Combine(EOSRoot, "Bin");

        return Directory.Exists(EOSRoot) && 
               Directory.Exists(EOSInclude) && 
               Directory.Exists(EOSLibDir) && 
               Directory.Exists(EOSBinDir);
    }

    public void AddEOS(ReadOnlyTargetRules Target)
    {
        var EOSRoot     = Path.Combine(ThirdPartyRoot, "EOS", "SDK");
        var EOSInclude  = Path.Combine(EOSRoot, "Include");
        var EOSLibDir   = Path.Combine(EOSRoot, "Lib");
        var EOSBinDir   = Path.Combine(EOSRoot, "Bin");

        PrivateIncludePaths.Add(EOSInclude);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            var lib = Path.Combine(EOSLibDir, "EOSSDK-Win64-Shipping.lib");
            var dll = Path.Combine(EOSBinDir, "EOSSDK-Win64-Shipping.dll");
            PublicAdditionalLibraries.Add(lib);
            RuntimeDependencies.Add("$(TargetOutputDir)/EOSSDK-Win64-Shipping.dll", dll);
            PublicDelayLoadDLLs.Add("EOSSDK-Win64-Shipping.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            RuntimeDependencies.Add($"{EOSBinDir}/libEOSSDK-Linux-Shipping.so");
        }
        else if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
        {
            RuntimeDependencies.Add($"{EOSBinDir}/libEOSSDK-LinuxArm64-Shipping.so");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            RuntimeDependencies.Add($"{EOSBinDir}/libEOSSDK-Mac-Shipping.dylib");
        }
    }

    public bool IsSteamworksAvailable()
    {
        string SteamRoot = Path.Combine(ThirdPartyRoot, "Steamworks", "sdk");
        string SteamInclude = Path.Combine(SteamRoot, "public");
        string SteamBinRoot = Path.Combine(SteamRoot, "redistributable_bin");

        return Directory.Exists(SteamRoot) && 
               Directory.Exists(SteamInclude) && 
               Directory.Exists(SteamBinRoot);
    }

    public void AddSteamworks(ReadOnlyTargetRules Target)
    {
        string SteamRoot     = Path.Combine(ThirdPartyRoot, "Steamworks", "sdk");
        string SteamInclude  = Path.Combine(SteamRoot, "public");
        string SteamBinRoot  = Path.Combine(SteamRoot, "redistributable_bin");

        PrivateIncludePaths.Add(SteamInclude);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string PlatformFolder  = Path.Combine(SteamRoot, "win64");
            RuntimeDependencies.Add($"{PlatformFolder}/steam_api64.dll");
            PublicAdditionalLibraries.Add(Path.Combine(SteamBinRoot, "win64", "steam_api64.lib"));
            PublicDelayLoadDLLs.Add("steam_api64.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            RuntimeDependencies.Add($"{SteamBinRoot}/linux64/libsteam_api.so");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            RuntimeDependencies.Add($"{SteamBinRoot}/osx/libsteam_api.dylib");
        }
    }

    public enum EServiceBackends
    {
        Steamworks,
        EpicOnlineServices,
    }

    public GamingServices(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "Projects",
            "Json",
            "JsonUtilities",
        });

        // Switch backend here
        const EServiceBackends backend = EServiceBackends.Steamworks;

        bool bServiceConfigured = false;
        switch (backend)
        {
            case EServiceBackends.EpicOnlineServices:
                if (IsEOSAvailable())
                {
                    PublicDefinitions.Add("USE_EOS");
                    AddEOS(Target);
                    bServiceConfigured = true;
                }
                break;
            case EServiceBackends.Steamworks:
                if (IsSteamworksAvailable())
                {
                    AddSteamworks(Target);
                    PublicDefinitions.Add("USE_STEAMWORKS");
                    bServiceConfigured = true;
                }
                break;
        }

        // Fallback to null service if no SDK is available
        if (!bServiceConfigured)
        {
            Console.WriteLine($"Warning: {backend} SDK not available. Falling back to null service.");
        }
    }
}

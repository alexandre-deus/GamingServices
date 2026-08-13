using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnrealBuildTool;

/**
 * GamingServices module rules.
 *
 * Every platform SDK that is vendored and supported on the target platform is compiled in.
 * GS_WITH_EOS / GS_WITH_STEAM mean "this backend is compiled in", NOT "this backend is the one being
 * used" — which backends actually run, and how they are wired together, is a profile declared in code
 * (Public/Native/GamingServiceProfile.h) and selected by the GS_PROFILE define.
 *
 * The profile is chosen HERE, in the build rules — never by editing a header. Producing distinct
 * builds (a pure-EOS one and a Steam-signs-in-EOS-runs-it one) is a one-line change in C#:
 *
 *   1. Per target, in a .Target.cs — this wins:
 *          ProjectDefinitions.Add("GS_PROFILE=SteamAuthIntoEpic");
 *   2. Otherwise DefaultProfile below applies, so a target that says nothing still builds.
 *
 * Both are tracked by UBT, so switching rebuilds what it must. An unknown profile name is a compile
 * error naming the bad profile. (Deliberately NOT an environment variable: UBT caches module rules
 * evaluation and does not treat the environment as a dependency, so an env-var switch silently fails
 * to take effect on an already-built tree.)
 *
 * A target may additionally exclude a backend from the build entirely — for a store build that must
 * not contain the other platform's binary at all:
 *
 *   ProjectDefinitions.Add("GS_EXCLUDE_STEAM=1");
 *
 * Rarely needed, since an unused backend costs only its (never loaded) staged library.
 *
 * Nothing here is bound at link time. No import library and no delay-load entry is added for either
 * SDK: the module resolves every SDK entry point at runtime (Private/Native/EOS/EOSDynamicApi.*,
 * Private/Native/Steam/SteamDynamicApi.cpp) out of ONE common folder that every backend's shared
 * libraries are staged into:
 *
 *     <Project>/Binaries/ThirdParty/GamingServices/<Platform>/
 *
 * That keeps a missing or unselected SDK library a runtime non-event (the backend reports unavailable)
 * instead of a process that refuses to start.
 */
public class GamingServices : ModuleRules
{
    /**
     * Backend arrangement used by any target that does not pin one of its own. Must name a profile
     * declared in Public/Native/GamingServiceProfile.h — a typo fails the C++ compile, loudly.
     */
    private const string DefaultProfile = "EpicOnly";

    /** Staging folder, relative to the project directory, shared by every backend's runtime libraries. */
    private const string CommonSdkDir = "Binaries/ThirdParty/GamingServices";

    private string PluginRoot => Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
    private string ProjectRoot => Path.GetFullPath(Path.Combine(PluginRoot, "..", ".."));

    /** Absolute path of the common folder for this target's platform. */
    private string CommonSdkDirFor(ReadOnlyTargetRules Target) =>
        Path.Combine(ProjectRoot, CommonSdkDir.Replace('/', Path.DirectorySeparatorChar), Target.Platform.ToString());

    /**
     * Stages a backend's runtime library into the common folder, and copies it there now so
     * editor / uncooked runs load the same file the packaged build will.
     */
    private void StageSdkLibrary(ReadOnlyTargetRules Target, string SourceFile)
    {
        string FileName = Path.GetFileName(SourceFile);

        if (!File.Exists(SourceFile))
        {
            Console.WriteLine($"[GamingServices]   ERROR: SDK library missing: {SourceFile}");
            return;
        }

        RuntimeDependencies.Add($"$(ProjectDir)/{CommonSdkDir}/{Target.Platform}/{FileName}", SourceFile);

        string DestDir = CommonSdkDirFor(Target);
        Directory.CreateDirectory(DestDir);
        try
        {
            File.Copy(SourceFile, Path.Combine(DestDir, FileName), overwrite: true);
            Console.WriteLine($"[GamingServices]   staged {FileName} -> {DestDir}");
        }
        catch (IOException)
        {
            // Already loaded by a running editor. The on-disk copy is the one we would have written.
            Console.WriteLine($"[GamingServices]   staged {FileName} (in use, kept existing copy)");
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Epic Online Services
    // ---------------------------------------------------------------------------------------------

    /** Android ships a separate, self-contained SDK tree (AAR + per-ABI .so, a newer EOS version). */
    private static bool IsAndroid(ReadOnlyTargetRules Target) => Target.Platform == UnrealTargetPlatform.Android;

    /**
     * iOS ships its own SDK download too: a single dynamic EOSSDK.framework that carries its headers
     * inside it. The tree is vendored exactly as extracted — see EOSIncludeDir for where that leaves
     * the headers, and the iOS branch of AddEOS for why the binary is linked rather than staged.
     */
    private static bool IsIOS(ReadOnlyTargetRules Target) => Target.Platform == UnrealTargetPlatform.IOS;

    private string EOSRoot(ReadOnlyTargetRules Target)
    {
        string Variant = IsAndroid(Target) ? "SDK-Android"
                       : IsIOS(Target)     ? "SDK-IOS"
                                           : "SDK";
        return Path.Combine(PluginRoot, "ThirdParty", "EOS", Variant);
    }

    /** The vendored iOS framework, as the download lays it out. Headers included, literally. */
    private string EOSIOSFrameworkDir(ReadOnlyTargetRules Target) =>
        Path.Combine(EOSRoot(Target), "Bin", "EOSSDK.framework");

    /**
     * Where this platform's EOS headers live in the vendored tree.
     *
     * Every SDK but one puts them in a sibling Include/ folder. The iOS download carries them inside
     * the framework instead (EOSSDK.framework/Headers), so that is what gets added to the include path
     * — pointed at directly rather than copied out to match the others. The vendored tree is then a
     * verbatim extraction on every platform, and an SDK upgrade is a re-extract with no fixup step
     * that can be forgotten or done wrong. Include style is unaffected: the headers are flat and
     * self-referential (#include "eos_sdk.h"), so only the directory differs.
     */
    private string EOSIncludeDir(ReadOnlyTargetRules Target) =>
        IsIOS(Target) ? Path.Combine(EOSIOSFrameworkDir(Target), "Headers")
                      : Path.Combine(EOSRoot(Target), "Include");

    private bool IsEOSVendored(ReadOnlyTargetRules Target)
    {
        bool bHasInclude = Directory.Exists(EOSIncludeDir(Target));
        bool bHasBin = Directory.Exists(Path.Combine(EOSRoot(Target), "Bin"));
        return bHasInclude && bHasBin;
    }

    /** Runtime library file name EOS is loaded by, or null where EOS has no vendored binary. */
    private static string EOSLibraryName(ReadOnlyTargetRules Target)
    {
        if (Target.Platform == UnrealTargetPlatform.Win64)      return "EOSSDK-Win64-Shipping.dll";
        if (Target.Platform == UnrealTargetPlatform.Mac)        return "libEOSSDK-Mac-Shipping.dylib";
        if (Target.Platform == UnrealTargetPlatform.Linux)      return "libEOSSDK-Linux-Shipping.so";
        if (Target.Platform == UnrealTargetPlatform.LinuxArm64) return "libEOSSDK-LinuxArm64-Shipping.so";
        if (Target.Platform == UnrealTargetPlatform.Android)    return "libEOSSDK.so";
        // Not a file that gets loaded on iOS — the framework is linked, so this only names the SDK in
        // logs. See the iOS branch of AddEOS and FGamingSdkLibrary::Load.
        if (Target.Platform == UnrealTargetPlatform.IOS)        return "EOSSDK.framework";
        return null;
    }

    /** Whether a target rules file asked for this backend to be left out of the build entirely. */
    private static bool IsBackendExcluded(ReadOnlyTargetRules Target, string Name)
    {
        return Target.ProjectDefinitions.Contains($"GS_EXCLUDE_{Name}=1");
    }

    private bool AddEOS(ReadOnlyTargetRules Target)
    {
        if (IsBackendExcluded(Target, "EOS"))
        {
            Console.WriteLine("[GamingServices] EOS: excluded by the target (GS_EXCLUDE_EOS).");
            return false;
        }

        string LibraryName = EOSLibraryName(Target);
        if (LibraryName == null || !IsEOSVendored(Target))
        {
            Console.WriteLine($"[GamingServices] EOS: not available on {Target.Platform}, not compiled in.");
            return false;
        }

        string Root = EOSRoot(Target);
        PrivateIncludePaths.Add(EOSIncludeDir(Target));

        if (IsAndroid(Target))
        {
            // libEOSSDK.so is packaged from the AAR by gradle and loaded by the SDK's own Java bootstrap
            // (EOSSDK.init, wired in EOS_Android_UPL.xml). Nothing to stage and nothing to link — the
            // runtime loader just dlopen()s the already-packaged library by name.
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginRoot, "EOS_Android_UPL.xml"));
        }
        else if (IsIOS(Target))
        {
            // iOS is the one platform that cannot resolve this backend the way the rest do. There is no
            // runtime library loading (FPlatformProcess::GetDllHandle is unimplemented and fatal there),
            // so the library cannot be staged into the common folder and dlopen'd. Embed the framework in
            // the .app and link it instead: dyld maps it before main(), and FGamingSdkLibrary resolves the
            // same symbol table straight out of the process image. Nothing about the call sites changes.
            string FrameworkZip = Path.Combine(Root, "Bin", "EOSSDK.embeddedframework.zip");
            if (!File.Exists(FrameworkZip))
            {
                // UBT wants the framework as a zip laid out <Name>.embeddedframework/<Name>.framework.
                Console.WriteLine($"[GamingServices]   ERROR: EOS iOS framework missing: {FrameworkZip}");
                return false;
            }

            PublicAdditionalFrameworks.Add(
                new Framework("EOSSDK", FrameworkZip, Framework.FrameworkMode.LinkAndCopy));

            // The account portal is presented through ASWebAuthenticationSession, which on iOS must be
            // told which window to present over (EOSIOSAuth.mm). That needs the protocol's own framework
            // and the engine's IOSAppDelegate, which lives in ApplicationCore.
            PublicFrameworks.Add("AuthenticationServices");
            PrivateDependencyModuleNames.Add("ApplicationCore");

            Console.WriteLine("[GamingServices]   embedded EOSSDK.framework (linked into the app, copied into the .app bundle)");
        }
        else
        {
            StageSdkLibrary(Target, Path.Combine(Root, "Bin", LibraryName));
        }

        PublicDefinitions.Add("GS_WITH_EOS=1");
        PublicDefinitions.Add($"GS_EOS_LIBRARY_NAME=\"{LibraryName}\"");
        Console.WriteLine($"[GamingServices] EOS: compiled in ({LibraryName}).");
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // Steamworks
    // ---------------------------------------------------------------------------------------------

    private string SteamRoot => Path.Combine(PluginRoot, "ThirdParty", "Steamworks", "sdk");

    private bool IsSteamVendored()
    {
        return Directory.Exists(Path.Combine(SteamRoot, "public"))
            && Directory.Exists(Path.Combine(SteamRoot, "redistributable_bin"));
    }

    /** (subdirectory of redistributable_bin, library file name) for the target platform. */
    private static Tuple<string, string> SteamLibrary(ReadOnlyTargetRules Target)
    {
        if (Target.Platform == UnrealTargetPlatform.Win64) return Tuple.Create("win64", "steam_api64.dll");
        if (Target.Platform == UnrealTargetPlatform.Mac)   return Tuple.Create("osx", "libsteam_api.dylib");
        if (Target.Platform == UnrealTargetPlatform.Linux) return Tuple.Create("linux64", "libsteam_api.so");
        return null;
    }

    private bool AddSteamworks(ReadOnlyTargetRules Target)
    {
        if (IsBackendExcluded(Target, "STEAM"))
        {
            Console.WriteLine("[GamingServices] Steamworks: excluded by the target (GS_EXCLUDE_STEAM).");
            return false;
        }

        Tuple<string, string> Library = SteamLibrary(Target);
        if (Library == null || !IsSteamVendored())
        {
            Console.WriteLine($"[GamingServices] Steamworks: not available on {Target.Platform}, not compiled in.");
            return false;
        }

        PrivateIncludePaths.Add(Path.Combine(SteamRoot, "public"));
        StageSdkLibrary(Target, Path.Combine(SteamRoot, "redistributable_bin", Library.Item1, Library.Item2));

        PublicDefinitions.Add("GS_WITH_STEAM=1");
        PublicDefinitions.Add($"GS_STEAM_LIBRARY_NAME=\"{Library.Item2}\"");
        // Steam's headers declare their global C entry points dllimport unless told otherwise. This module
        // supplies those entry points itself (SteamDynamicApi.cpp) as forwarders onto runtime-resolved
        // symbols, so they must be declared with plain extern "C" linkage instead.
        PublicDefinitions.Add("STEAM_API_NODLL=1");
        PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
        Console.WriteLine($"[GamingServices] Steamworks: compiled in ({Library.Item2}).");
        return true;
    }

    // ---------------------------------------------------------------------------------------------

    public GamingServices(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Per-backend private roots so internal headers resolve by name across the subfolders
        // (e.g. "SteamPlatformCore.h", "EOSCommon.h", "GamingSdkLibrary.h").
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "Native", "SDK"));
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

        // Where the runtime loader looks for every backend's shared library. The platform folder is
        // passed through rather than derived at runtime so the loader and the staging step can never
        // disagree about the spelling (Win64 / Mac / Linux / LinuxArm64 / Android).
        PublicDefinitions.Add($"GS_SDK_COMMON_DIR=\"{CommonSdkDir}\"");
        PublicDefinitions.Add($"GS_SDK_PLATFORM_DIR=\"{Target.Platform}\"");

        // A target that pins its own profile already reaches the compiler through ProjectDefinitions;
        // defining ours as well would be a macro redefinition. So supply DefaultProfile only when the
        // target stayed silent. Either way the build log records which arrangement it produced.
        string PinnedProfile = Target.ProjectDefinitions.FirstOrDefault(Definition => Definition.StartsWith("GS_PROFILE="));
        if (PinnedProfile != null)
        {
            Console.WriteLine($"[GamingServices] Profile: {PinnedProfile.Substring("GS_PROFILE=".Length)} (pinned by the target).");
        }
        else
        {
            PublicDefinitions.Add($"GS_PROFILE={DefaultProfile}");
            Console.WriteLine($"[GamingServices] Profile: {DefaultProfile} (GamingServices.Build.cs default).");
        }

        List<string> CompiledIn = new List<string>();
        if (AddEOS(Target))        CompiledIn.Add("EOS");
        if (AddSteamworks(Target)) CompiledIn.Add("Steamworks");

        Console.WriteLine(CompiledIn.Count > 0
            ? $"[GamingServices] Backends compiled in: {string.Join(", ", CompiledIn)}."
            : "[GamingServices] No platform SDK compiled in; only the null backend is available.");
    }
}

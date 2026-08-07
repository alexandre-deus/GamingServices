#include "GamingSdkLibrary.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#if PLATFORM_IOS
#include <dlfcn.h>
#endif

#ifndef GS_SDK_COMMON_DIR
#define GS_SDK_COMMON_DIR "Binaries/ThirdParty/GamingServices"
#endif

#ifndef GS_SDK_PLATFORM_DIR
#define GS_SDK_PLATFORM_DIR "Unknown"
#endif

namespace GamingServices
{
	FString FGamingSdkLibrary::GetCommonDirectory()
	{
		// Absolute: the DLL directory push below and FPlatformProcess::GetDllHandle both want a real
		// path, and ProjectDir() is relative in packaged builds ("../../../TurtleRock/").
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir() / TEXT(GS_SDK_COMMON_DIR) / TEXT(GS_SDK_PLATFORM_DIR)) / TEXT("");
	}

	bool FGamingSdkLibrary::Load()
	{
		if (Handle)
		{
			return true;
		}
		if (bLoadFailed)
		{
			return false;
		}

#if PLATFORM_IOS
		// iOS does not load libraries at runtime, and must not be asked to: GetDllHandle is not
		// implemented there (FIOSPlatformProcess inherits the generic one, which logs Fatal and takes
		// the process down), so the staging-folder path below is not merely useless on iOS but unsafe.
		//
		// An iOS SDK ships as a dynamic framework that GamingServices.Build.cs embeds in the .app and
		// links, so dyld has already mapped it before main() runs. Resolving against the process image
		// therefore gives exactly what a successful load gives everywhere else, and this class's
		// contract is unchanged. A framework that is absent resolves no symbols, which the callers
		// already treat as an unusable SDK and report as the backend being unavailable.
		Handle = RTLD_DEFAULT;
		LoadedPath = TEXT("<linked into the application>");
		UE_LOG(LogTemp, Log, TEXT("GamingServices: SDK library '%s' is linked into the application"), *LibraryName);
		return true;
#else
		const FString CommonDir = GetCommonDirectory();
		const FString FullPath = CommonDir / LibraryName;

		// Push the common folder as a DLL search directory for the duration of the load so a library
		// that pulls in siblings staged next to it resolves them from there too.
		FPlatformProcess::PushDllDirectory(*CommonDir);
		if (FPaths::FileExists(FullPath))
		{
			Handle = FPlatformProcess::GetDllHandle(*FullPath);
			if (Handle)
			{
				LoadedPath = FullPath;
			}
		}

		if (!Handle)
		{
			// Fall back to the platform loader's own search path. This is the normal path on Android,
			// where the EOS library lives inside the APK and is already loaded by the SDK's Java
			// bootstrap, and it also picks up a library a host process loaded before us.
			Handle = FPlatformProcess::GetDllHandle(*LibraryName);
		}
		FPlatformProcess::PopDllDirectory(*CommonDir);

		if (!Handle)
		{
			bLoadFailed = true;
			UE_LOG(LogTemp, Log,
			       TEXT("GamingServices: SDK library '%s' not loadable (looked in '%s' and on the system path); "
				       "its backend will report unavailable."),
			       *LibraryName, *CommonDir);
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("GamingServices: loaded SDK library '%s' from '%s'"),
		       *LibraryName, LoadedPath.IsEmpty() ? TEXT("<system search path>") : *LoadedPath);
		return true;
#endif // !PLATFORM_IOS
	}

	void FGamingSdkLibrary::Unload()
	{
		if (Handle)
		{
#if !PLATFORM_IOS
			// Nothing was opened on iOS, so there is no handle to release — and FreeDllHandle is the
			// same unimplemented generic entry point as GetDllHandle, so calling it would be fatal.
			FPlatformProcess::FreeDllHandle(Handle);
#endif
			Handle = nullptr;
			LoadedPath.Empty();
		}
		bLoadFailed = false;
	}

	void* FGamingSdkLibrary::GetExport(const TCHAR* SymbolName) const
	{
		if (!Handle)
		{
			return nullptr;
		}
#if PLATFORM_IOS
		return dlsym(Handle, TCHAR_TO_ANSI(SymbolName));
#else
		return FPlatformProcess::GetDllExport(Handle, SymbolName);
#endif
	}
}

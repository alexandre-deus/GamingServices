#include "GamingSdkLibrary.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

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
	}

	void FGamingSdkLibrary::Unload()
	{
		if (Handle)
		{
			FPlatformProcess::FreeDllHandle(Handle);
			Handle = nullptr;
			LoadedPath.Empty();
		}
		bLoadFailed = false;
	}

	void* FGamingSdkLibrary::GetExport(const TCHAR* SymbolName) const
	{
		return Handle ? FPlatformProcess::GetDllExport(Handle, SymbolName) : nullptr;
	}
}

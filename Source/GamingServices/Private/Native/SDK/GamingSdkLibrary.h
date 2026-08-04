#pragma once

#include "CoreMinimal.h"

namespace GamingServices
{
	/**
	 * Runtime handle to one platform SDK's shared library.
	 *
	 * No backend is bound at link time. Every SDK entry point this module calls is resolved through one
	 * of these at runtime, out of the common staging folder that GamingServices.Build.cs copies every
	 * compiled-in backend's library into:
	 *
	 *     <Project>/Binaries/ThirdParty/GamingServices/<Platform>/
	 *
	 * The practical consequence is that a library which is absent (a build shipped without Steam, a
	 * player with no Steam client, an EOS-less store build) makes its backend report unavailable at
	 * runtime instead of preventing the process from starting.
	 */
	class FGamingSdkLibrary
	{
	public:
		explicit FGamingSdkLibrary(const TCHAR* InLibraryName)
			: LibraryName(InLibraryName)
		{
		}

		~FGamingSdkLibrary()
		{
			Unload();
		}

		FGamingSdkLibrary(const FGamingSdkLibrary&) = delete;
		FGamingSdkLibrary& operator=(const FGamingSdkLibrary&) = delete;

		/**
		 * Loads the library if it is not loaded already. Idempotent, and cheap to call repeatedly: a
		 * failed load is remembered so a missing library is not re-probed on every access.
		 */
		bool Load();

		void Unload();

		bool IsLoaded() const { return Handle != nullptr; }

		/** Resolved address of an exported symbol, or null when the library is not loaded / lacks it. */
		void* GetExport(const TCHAR* SymbolName) const;

		const FString& GetLibraryName() const { return LibraryName; }

		/** Absolute path the library was loaded from; empty when it came off the system search path. */
		const FString& GetLoadedPath() const { return LoadedPath; }

		/** <Project>/Binaries/ThirdParty/GamingServices/<Platform>/ — the common folder, with trailing slash. */
		static FString GetCommonDirectory();

	private:
		FString LibraryName;
		FString LoadedPath;
		void* Handle = nullptr;
		bool bLoadFailed = false;
	};
}

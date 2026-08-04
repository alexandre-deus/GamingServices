#include "Native/GamingServiceFactory.h"

#include "Native/Composite/CompositeGamingService.h"
#include "Native/Null/NullGamingService.h"

#if defined(GS_WITH_EOS)
#include "Native/EOS/EOSGamingService.h"
#endif

#if defined(GS_WITH_STEAM)
#include "Native/Steam/SteamGamingService.h"
#endif

namespace GamingServices
{
	TUniquePtr<IGamingService> CreateBackendService(EGamingBackend Backend)
	{
		if (!IsBackendAvailable(Backend))
		{
			return nullptr;
		}

		switch (Backend)
		{
#if defined(GS_WITH_EOS)
		case EGamingBackend::EpicOnlineServices:
			return MakeUnique<FEOSGamingService>();
#endif
#if defined(GS_WITH_STEAM)
		case EGamingBackend::Steamworks:
			return MakeUnique<FSteamGamingService>();
#endif
		default:
			return nullptr;
		}
	}

	EGamingBackend ResolvePrimaryBackend(const FGamingServicesRuntimeConfig& Config)
	{
		for (EGamingBackend Backend : Config.Backends)
		{
			if (IsBackendAvailable(Backend))
			{
				return Backend;
			}
			UE_LOG(LogTemp, Log, TEXT("GamingServices: %s is not available here; trying the next preference"),
			       LexToString(Backend));
		}
		return EGamingBackend::None;
	}

	TUniquePtr<IGamingService> CreateGamingService(const FGamingServicesRuntimeConfig& Config)
	{
		const EGamingBackend Primary = ResolvePrimaryBackend(Config);
		if (Primary == EGamingBackend::None)
		{
			UE_LOG(LogTemp, Warning, TEXT("GamingServices: no configured backend is available; using the null backend"));
			return MakeUnique<FNullGamingService>();
		}

		TUniquePtr<IGamingService> PrimaryService = CreateBackendService(Primary);
		if (!PrimaryService)
		{
			UE_LOG(LogTemp, Error,
			       TEXT("GamingServices: %s reported available but could not be created; using the null backend"),
			       LexToString(Primary));
			return MakeUnique<FNullGamingService>();
		}

		// A configuration that does not wire backends together gets the backend itself, unwrapped. This is
		// the common case and it stays exactly as cheap as a single-backend build was.
		if (!Config.RequiresCompositeService(Primary))
		{
			UE_LOG(LogTemp, Log, TEXT("GamingServices: using %s"), LexToString(Primary));
			return PrimaryService;
		}

		TArray<FCompositeGamingService::FBackendEntry> Entries;
		Entries.Add({Primary, MoveTemp(PrimaryService)});

		for (EGamingBackend Backend : Config.GetReferencedBackends())
		{
			if (Backend == Primary)
			{
				continue;
			}
			if (TUniquePtr<IGamingService> Service = CreateBackendService(Backend))
			{
				Entries.Add({Backend, MoveTemp(Service)});
			}
			else
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("GamingServices: %s is referenced by the configuration but unavailable; %s will cover for it"),
				       LexToString(Backend), LexToString(Primary));
			}
		}

		// Everything the config asked to combine with turned out to be unavailable, so there is nothing
		// left to compose — hand back the single backend rather than an empty wrapper.
		if (Entries.Num() == 1)
		{
			UE_LOG(LogTemp, Log, TEXT("GamingServices: using %s (no secondary backend available)"), LexToString(Primary));
			return MoveTemp(Entries[0].Service);
		}

		UE_LOG(LogTemp, Log, TEXT("GamingServices: composite service — %s"), *Config.ToString());
		return MakeUnique<FCompositeGamingService>(Config, MoveTemp(Entries));
	}

	TUniquePtr<IGamingService> CreateGamingService()
	{
		return CreateGamingService(FGamingServicesRuntimeConfig::Active());
	}
}

#include "Native/GamingBackend.h"

#include "Native/GamingServiceProfile.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if defined(GS_WITH_EOS)
#include "EOSDynamicApi.h"
#endif

#if defined(GS_WITH_STEAM)
#include "SteamDynamicApi.h"
#endif

namespace GamingServices
{
	const TCHAR* LexToString(EGamingBackend Backend)
	{
		switch (Backend)
		{
		case EGamingBackend::Steamworks:         return TEXT("Steamworks");
		case EGamingBackend::EpicOnlineServices: return TEXT("EpicOnlineServices");
		default:                                 return TEXT("None");
		}
	}

	bool TryParseGamingBackend(const FString& Text, EGamingBackend& OutBackend)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.Equals(TEXT("Steam"), ESearchCase::IgnoreCase) ||
			Trimmed.Equals(TEXT("Steamworks"), ESearchCase::IgnoreCase))
		{
			OutBackend = EGamingBackend::Steamworks;
			return true;
		}
		if (Trimmed.Equals(TEXT("EOS"), ESearchCase::IgnoreCase) ||
			Trimmed.Equals(TEXT("Epic"), ESearchCase::IgnoreCase) ||
			Trimmed.Equals(TEXT("EpicOnlineServices"), ESearchCase::IgnoreCase))
		{
			OutBackend = EGamingBackend::EpicOnlineServices;
			return true;
		}
		if (Trimmed.Equals(TEXT("None"), ESearchCase::IgnoreCase) ||
			Trimmed.Equals(TEXT("Null"), ESearchCase::IgnoreCase))
		{
			OutBackend = EGamingBackend::None;
			return true;
		}
		return false;
	}

	bool IsBackendCompiledIn(EGamingBackend Backend)
	{
		switch (Backend)
		{
#if defined(GS_WITH_EOS)
		case EGamingBackend::EpicOnlineServices: return true;
#endif
#if defined(GS_WITH_STEAM)
		case EGamingBackend::Steamworks: return true;
#endif
		default: return false;
		}
	}

	bool IsBackendAvailable(EGamingBackend Backend)
	{
		switch (Backend)
		{
#if defined(GS_WITH_EOS)
		case EGamingBackend::EpicOnlineServices: return LoadEOSApi();
#endif
#if defined(GS_WITH_STEAM)
		case EGamingBackend::Steamworks: return IsSteamApiAvailable();
#endif
		default: return false;
		}
	}
}

FGamingServicesRuntimeConfig FGamingServicesRuntimeConfig::FromProfile(const FGamingServiceProfile& Profile)
{
	FGamingServicesRuntimeConfig Config;
	Config.ProfileName = Profile.Name;
	Config.AuthBackend = Profile.AuthBackend;
	Config.bAllowAuthFallback = Profile.bAllowAuthFallback;

	for (EGamingBackend Backend : Profile.Backends)
	{
		if (Backend != EGamingBackend::None)
		{
			Config.Backends.AddUnique(Backend);
		}
	}

	for (const FGamingCapabilityOverride& Override : Profile.CapabilityOverrides)
	{
		if (Override.Backend != EGamingBackend::None)
		{
			Config.CapabilityOverrides.Add(Override);
		}
	}

	return Config;
}

FGamingServicesRuntimeConfig FGamingServicesRuntimeConfig::Active()
{
	FGamingServicesRuntimeConfig Config = FromProfile(GetActiveGamingServiceProfile());

	// Debug override: run this exact build against a single backend without rebuilding it. Deliberately
	// command-line only — the compiled-in profile is what ships, this is for isolating a backend during
	// testing. It also drops the cross-backend wiring, since exercising one backend alone is the point.
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("GamingBackend="), Override))
	{
		EGamingBackend Backend = EGamingBackend::None;
		if (GamingServices::TryParseGamingBackend(Override, Backend))
		{
			Config.Backends.Reset();
			if (Backend != EGamingBackend::None)
			{
				Config.Backends.Add(Backend);
			}
			Config.AuthBackend = EGamingBackend::None;
			Config.CapabilityOverrides.Reset();
			Config.ProfileName += FString::Printf(TEXT(" (overridden to %s)"), GamingServices::LexToString(Backend));

			UE_LOG(LogTemp, Warning, TEXT("GamingServices: -GamingBackend=%s overrides the compiled-in profile"),
			       GamingServices::LexToString(Backend));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GamingServices: ignoring unknown -GamingBackend=%s"), *Override);
		}
	}

	return Config;
}

TArray<EGamingBackend> FGamingServicesRuntimeConfig::GetReferencedBackends() const
{
	TArray<EGamingBackend> Referenced = Backends;
	if (AuthBackend != EGamingBackend::None)
	{
		Referenced.AddUnique(AuthBackend);
	}
	for (const FGamingCapabilityOverride& Override : CapabilityOverrides)
	{
		if (Override.Backend != EGamingBackend::None)
		{
			Referenced.AddUnique(Override.Backend);
		}
	}
	return Referenced;
}

EGamingBackend FGamingServicesRuntimeConfig::GetBackendForCapability(EGamingCapability Capability, EGamingBackend Primary) const
{
	for (const FGamingCapabilityOverride& Override : CapabilityOverrides)
	{
		if (Override.Capability == Capability && Override.Backend != EGamingBackend::None)
		{
			return Override.Backend;
		}
	}
	return Primary;
}

bool FGamingServicesRuntimeConfig::RequiresCompositeService(EGamingBackend Primary) const
{
	if (AuthBackend != EGamingBackend::None && AuthBackend != Primary)
	{
		return true;
	}
	for (const FGamingCapabilityOverride& Override : CapabilityOverrides)
	{
		if (Override.Backend != EGamingBackend::None && Override.Backend != Primary)
		{
			return true;
		}
	}
	return false;
}

FString FGamingServicesRuntimeConfig::ToString() const
{
	TArray<FString> BackendNames;
	for (EGamingBackend Backend : Backends)
	{
		BackendNames.Add(GamingServices::LexToString(Backend));
	}

	FString Result = FString::Printf(TEXT("profile '%s': Backends=[%s] Auth=%s"),
	                                 *ProfileName,
	                                 *FString::Join(BackendNames, TEXT(", ")),
	                                 GamingServices::LexToString(AuthBackend));

	if (CapabilityOverrides.Num() > 0)
	{
		const UEnum* CapabilityEnum = StaticEnum<EGamingCapability>();
		TArray<FString> OverrideNames;
		for (const FGamingCapabilityOverride& Override : CapabilityOverrides)
		{
			OverrideNames.Add(FString::Printf(TEXT("%s->%s"),
			                                  CapabilityEnum ? *CapabilityEnum->GetNameStringByValue(static_cast<int64>(Override.Capability)) : TEXT("?"),
			                                  GamingServices::LexToString(Override.Backend)));
		}
		Result += FString::Printf(TEXT(" Overrides=[%s]"), *FString::Join(OverrideNames, TEXT(", ")));
	}

	return Result;
}

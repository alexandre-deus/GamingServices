#include "Native/Composite/CompositeGamingService.h"

#include "Native/Composite/CompositeMatchmaking.h"
#include "Native/Composite/CompositeUser.h"
#include "Native/Interfaces/IExternalAuthService.h"
#include "Native/Interfaces/IInviteTransport.h"
#include "Native/Interfaces/IMatchmakingService.h"

#include "HAL/PlatformMisc.h"

namespace GamingServices
{
	FCompositeGamingService::FCompositeGamingService(const FGamingServicesRuntimeConfig& InConfig,
	                                                 TArray<FBackendEntry> InBackends)
		: Config(InConfig)
		, Backends(MoveTemp(InBackends))
	{
		checkf(Backends.Num() > 0, TEXT("FCompositeGamingService requires at least one backend"));
		Primary = Backends[0].Backend;

		if (Config.AuthBackend != EGamingBackend::None && Config.AuthBackend != Primary)
		{
			IGamingService* PrimaryService = GetPrimaryService();
			IGamingService* IdentityService = GetBackendService(Config.AuthBackend);
			if (PrimaryService && IdentityService)
			{
				User = MakeUnique<FCompositeUser>(*PrimaryService, IdentityService, Config.bAllowAuthFallback);
				UE_LOG(LogTemp, Log, TEXT("GamingServices: %s authenticates, %s owns the session"),
				       LexToString(Config.AuthBackend), LexToString(Primary));
			}
		}

		// Invites are social, and under external auth the player's friends live on the identity backend
		// rather than the one running the session. Wrap the primary's matchmaking so its lobby ids travel
		// over that platform's invite system.
		IGamingService* PrimaryService = GetPrimaryService();
		IMatchmakingService* PrimaryMatchmaking = PrimaryService ? PrimaryService->GetMatchmaking() : nullptr;
		if (PrimaryMatchmaking)
		{
			if (IInviteTransport* Transport = GetInviteTransport())
			{
				Matchmaking = MakeUnique<FCompositeMatchmaking>(*PrimaryMatchmaking, Transport);
				UE_LOG(LogTemp, Log, TEXT("GamingServices: %s carries invites for %s sessions"),
				       LexToString(Config.AuthBackend != EGamingBackend::None ? Config.AuthBackend : Primary),
				       LexToString(Primary));
			}
		}
	}

	FCompositeGamingService::~FCompositeGamingService()
	{
		// Drop the wrappers before the backends they hold references and sinks into.
		Matchmaking.Reset();
		User.Reset();
	}

	IGamingService* FCompositeGamingService::GetPrimaryService() const
	{
		return Backends.Num() > 0 ? Backends[0].Service.Get() : nullptr;
	}

	IGamingService* FCompositeGamingService::GetBackendService(EGamingBackend Backend) const
	{
		for (const FBackendEntry& Entry : Backends)
		{
			if (Entry.Backend == Backend)
			{
				return Entry.Service.Get();
			}
		}
		return nullptr;
	}

	EGamingBackend FCompositeGamingService::GetBackend() const
	{
		return Primary;
	}

	void FCompositeGamingService::InitializePlatform(const FGamingServiceConnectParams& Params)
	{
		for (FBackendEntry& Entry : Backends)
		{
			if (Entry.Service)
			{
				Entry.Service->InitializePlatform(Params);
			}
		}

		EnforceAuthBackendRequirement();
	}

	void FCompositeGamingService::EnforceAuthBackendRequirement() const
	{
		// With fallback disabled the profile states a requirement, not a preference. Catch it here, at
		// init, rather than at the login call: by then the menu is up and the player is told "sign-in
		// failed" for what is really a broken build or a missing platform client.
		if (Config.AuthBackend == EGamingBackend::None || Config.bAllowAuthFallback)
		{
			return;
		}

		const IGamingService* IdentityService = GetBackendService(Config.AuthBackend);
		if (IdentityService && IdentityService->IsInitialized())
		{
			return;
		}

		UE_LOG(LogTemp, Error,
		       TEXT("GamingServices: profile '%s' requires %s to authenticate the %s session, but %s did not initialize. ")
		       TEXT("Fallback is disabled for this profile, so there is no identity to run as — terminating."),
		       *Config.ProfileName, LexToString(Config.AuthBackend), LexToString(Primary),
		       LexToString(Config.AuthBackend));

		// Forced, not cooperative. This runs from FGamingServicesModule::StartupModule, and a requested
		// exit at that point unwinds the engine before every module has registered its generated code —
		// which asserts in UObjectGlobals instead of quitting.
		FPlatformMisc::RequestExit(true);
	}

	void FCompositeGamingService::DestroyPlatform()
	{
		// The wrappers hold sinks on the backends; retire them first.
		Matchmaking.Reset();
		User.Reset();

		// Reverse order, so the primary (which the others were wired into) goes last.
		for (int32 Index = Backends.Num() - 1; Index >= 0; --Index)
		{
			if (Backends[Index].Service)
			{
				Backends[Index].Service->DestroyPlatform();
			}
		}
	}

	void FCompositeGamingService::Tick()
	{
		// Every backend ticks, not just the primary: the identity backend's SDK callbacks (the auth
		// ticket among them) are only delivered while it is being pumped.
		for (FBackendEntry& Entry : Backends)
		{
			if (Entry.Service)
			{
				Entry.Service->Tick();
			}
		}
	}

	bool FCompositeGamingService::IsInitialized() const
	{
		IGamingService* PrimaryService = GetPrimaryService();
		return PrimaryService && PrimaryService->IsInitialized();
	}

	template <typename TCapability>
	TCapability* FCompositeGamingService::RouteCapability(EGamingCapability Capability,
	                                                      TCapability* (IGamingService::*Accessor)() const) const
	{
		const EGamingBackend Target = Config.GetBackendForCapability(Capability, Primary);
		if (Target != Primary)
		{
			if (IGamingService* Routed = GetBackendService(Target))
			{
				if (TCapability* Result = (Routed->*Accessor)())
				{
					return Result;
				}
				UE_LOG(LogTemp, Verbose,
				       TEXT("GamingServices: %s does not implement the routed capability; using %s"),
				       LexToString(Target), LexToString(Primary));
			}
		}

		IGamingService* PrimaryService = GetPrimaryService();
		return PrimaryService ? (PrimaryService->*Accessor)() : nullptr;
	}

	IAchievementsService* FCompositeGamingService::GetAchievements() const
	{
		return RouteCapability(EGamingCapability::Achievements, &IGamingService::GetAchievements);
	}

	IEntitlementsService* FCompositeGamingService::GetEntitlements() const
	{
		return RouteCapability(EGamingCapability::Entitlements, &IGamingService::GetEntitlements);
	}

	ILeaderboardsService* FCompositeGamingService::GetLeaderboards() const
	{
		return RouteCapability(EGamingCapability::Leaderboards, &IGamingService::GetLeaderboards);
	}

	IStatsService* FCompositeGamingService::GetStats() const
	{
		return RouteCapability(EGamingCapability::Stats, &IGamingService::GetStats);
	}

	ICloudStorageService* FCompositeGamingService::GetCloudStorage() const
	{
		return RouteCapability(EGamingCapability::CloudStorage, &IGamingService::GetCloudStorage);
	}

	IRemoteSettingsService* FCompositeGamingService::GetRemoteSettings() const
	{
		return RouteCapability(EGamingCapability::RemoteSettings, &IGamingService::GetRemoteSettings);
	}

	IMatchmakingService* FCompositeGamingService::GetMatchmaking() const
	{
		// The invite-carrying wrapper wins when present — it is the only thing that knows how to get the
		// primary's lobby id in front of friends who are on a different platform.
		if (Matchmaking)
		{
			return Matchmaking.Get();
		}
		return RouteCapability(EGamingCapability::Matchmaking, &IGamingService::GetMatchmaking);
	}

	IInviteTransport* FCompositeGamingService::GetInviteTransport() const
	{
		// Prefer the backend the player is socially present on — the one that authenticated them.
		if (Config.AuthBackend != EGamingBackend::None)
		{
			if (IGamingService* IdentityService = GetBackendService(Config.AuthBackend))
			{
				if (IInviteTransport* Transport = IdentityService->GetInviteTransport())
				{
					return Transport;
				}
			}
		}
		IGamingService* PrimaryService = GetPrimaryService();
		return PrimaryService ? PrimaryService->GetInviteTransport() : nullptr;
	}

	IFriendsService* FCompositeGamingService::GetFriends() const
	{
		// Same reasoning as GetInviteTransport, and it matters more here: the social graph belongs to the
		// backend that authenticated the player. Under Steam-authenticates/EOS-hosts, EOS deliberately
		// reports no friends capability (a Connect-only user has no EpicAccountId, so no Epic social
		// graph), and Steam is the only backend that can answer — so this must not default to the primary.
		if (Config.AuthBackend != EGamingBackend::None)
		{
			if (IGamingService* IdentityService = GetBackendService(Config.AuthBackend))
			{
				if (IFriendsService* Friends = IdentityService->GetFriends())
				{
					return Friends;
				}
			}
		}
		return RouteCapability(EGamingCapability::Friends, &IGamingService::GetFriends);
	}

	IUserService* FCompositeGamingService::GetUser() const
	{
		// The bridged user wins when an auth backend is configured — it is the only thing that knows how
		// to turn one backend's credential into the other's session.
		if (User)
		{
			return User.Get();
		}
		return RouteCapability(EGamingCapability::User, &IGamingService::GetUser);
	}

	IP2PTransport* FCompositeGamingService::GetP2PTransport()
	{
		// Never routed: peers are addressed by the primary's identity namespace, the same one matchmaking
		// hands out. A transport from another backend could not address them.
		IGamingService* PrimaryService = GetPrimaryService();
		return PrimaryService ? PrimaryService->GetP2PTransport() : nullptr;
	}

	IExternalAuthProvider* FCompositeGamingService::GetExternalAuthProvider() const
	{
		if (Config.AuthBackend != EGamingBackend::None)
		{
			if (IGamingService* IdentityService = GetBackendService(Config.AuthBackend))
			{
				return IdentityService->GetExternalAuthProvider();
			}
		}
		IGamingService* PrimaryService = GetPrimaryService();
		return PrimaryService ? PrimaryService->GetExternalAuthProvider() : nullptr;
	}

	IExternalAuthConsumer* FCompositeGamingService::GetExternalAuthConsumer() const
	{
		IGamingService* PrimaryService = GetPrimaryService();
		return PrimaryService ? PrimaryService->GetExternalAuthConsumer() : nullptr;
	}
}

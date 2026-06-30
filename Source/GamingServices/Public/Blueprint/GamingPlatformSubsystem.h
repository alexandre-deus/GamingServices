#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Native/GamingCapability.h"
#include "Native/IGamingService.h"
#include "GamingPlatformSubsystem.generated.h"

/**
 * OOP capability-oriented gaming-services subsystem — the game's live access point to the native
 * backend.
 *
 * It does NOT own the backend: FGamingServicesModule owns and initializes the single platform service
 * (so exactly one backend inits the SDK). This subsystem borrows that instance, ticks it each frame,
 * exposes capability discovery (GetCapabilities()/HasCapability()), and resolves the backend from a
 * world context for the capability libraries.
 *
 * It carries NO per-capability operations or events — operations live in the per-capability Blueprint
 * libraries (Libraries/), and C++ callers reach interface sinks/operations through
 * GetService()->GetMatchmaking()/... directly.
 */
UCLASS()
class GAMINGSERVICES_API UGamingPlatformSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** The native backend. C++ callers use its per-interface accessors (GetMatchmaking(), ...) directly. */
	IGamingService* GetService() const { return Service; }

	/** Resolves the native backend from any world context object (used by the capability libraries). */
	static IGamingService* GetServiceFromContext(const UObject* WorldContextObject);

	/** Flat snapshot of everything the active backend supports (derived from the native accessors). */
	UFUNCTION(BlueprintPure, Category = "GamingServices")
	FGamingServiceCapabilities GetCapabilities() const { return Service ? Service->GetCapabilities() : FGamingServiceCapabilities(); }

	/** Whether the active backend supports a single capability. */
	UFUNCTION(BlueprintPure, Category = "GamingServices")
	bool HasCapability(EGamingCapability Capability) const { return GetCapabilities().Has(Capability); }

private:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject — pumps the platform SDK callbacks each frame.
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Service != nullptr; }

	// Non-owning; the live service is owned by FGamingServicesModule and outlives this subsystem.
	IGamingService* Service = nullptr;
};

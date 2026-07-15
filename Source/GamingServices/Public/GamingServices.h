// Copyright Epic Games, Inc.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"
#include "Native/IGamingService.h"

class GAMINGSERVICES_API FGamingServicesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** The single live platform backend. Initialized in StartupModule, torn down on engine pre-exit. */
	IGamingService& GetService() const { return *Service; }

	/**
	 * Null-safe access to the P2P transport. Returns nullptr once the platform has been torn down, so
	 * net-driver sockets that outlive teardown (destroyed by GC after OnEnginePreExit) never touch a
	 * freed transport.
	 */
	IP2PTransport* GetP2PTransportOrNull() const { return Service ? Service->GetP2PTransport() : nullptr; }

private:
	void TearDownPlatform();
	void TearDownSocketSubsystem();

	TUniquePtr<IGamingService> Service;
	FDelegateHandle PreExitHandle;
	bool bSocketSubsystemEnabled = false;
};

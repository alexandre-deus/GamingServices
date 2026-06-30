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

private:
	void TearDownPlatform();

	TUniquePtr<IGamingService> Service;
	FDelegateHandle PreExitHandle;
	bool bSocketSubsystemEnabled = false;
};

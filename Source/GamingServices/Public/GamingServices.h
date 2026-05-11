// Copyright Epic Games, Inc.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"
#include "Services/FGamingService.h"

class GAMINGSERVICES_API FGamingServicesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FGamingService& GetService() const { return *Service; }

private:
	void TearDownPlatform();

	TUniquePtr<FGamingService> Service;
	FDelegateHandle PreExitHandle;
	bool bSocketSubsystemEnabled = false;
};

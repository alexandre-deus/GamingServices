// Copyright Epic Games, Inc.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IGamingService;

class GAMINGSERVICES_API FGamingServicesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool bSocketSubsystemEnabled = false;
};

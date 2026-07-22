#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GamingServiceAsyncAction.generated.h"

class IGamingService;

/**
 * Shared base for the capability async-action nodes.
 *
 * Async-action nodes already get a built-in blank "then" exec pin that fires synchronously the instant
 * the node runs — that is the fire-and-forget continuation, so we do NOT declare our own (a delegate
 * named "Then" collides with the engine's reserved `then` pin and produces a duplicate, unlabeled
 * output). Each subclass adds a single `Completed` pin that carries the result struct when the platform
 * call returns; the result structs derive from FGamingServiceResult, so callers branch on
 * Result.bSuccess instead of separate success/failure pins.
 */
UCLASS(Abstract)
class GAMINGSERVICES_API UGamingServiceAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	TWeakObjectPtr<UObject> WorldContext;

	/** Resolves the live native backend from the captured world context. */
	IGamingService* ResolveService() const;

	/** Roots this action to the game instance so it survives until the async callback fires. */
	void KeepAlive();
};

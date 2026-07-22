#pragma once

#include "CoreMinimal.h"
#include "DataTypes/GamingServiceResult.h"
#include "AchievementTypes.generated.h"

USTRUCT(BlueprintType)
struct FGameAchievement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	bool bIsUnlocked = false;

	UPROPERTY(BlueprintReadOnly)
	double Progress = 0.0;
};

USTRUCT(BlueprintType)
struct FAchievementsQueryResult : public FGamingServiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FGameAchievement> Achievements;

	FAchievementsQueryResult() = default;

	FAchievementsQueryResult(bool InSuccess,
	                         const TArray<FGameAchievement>& InAchievements = TArray<FGameAchievement>())
		: FGamingServiceResult(InSuccess)
		  , Achievements(InAchievements)
	{
	}
};

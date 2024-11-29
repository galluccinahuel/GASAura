
#pragma once

#include "CoreMinimal.h"
#include "Character/AuraCharacterBase.h"
#include "Interfaces/EnemyInterface.h"
#include "AuraEnemy.generated.h"

UCLASS()
class AURA_API AAuraEnemy : public AAuraCharacterBase, public IEnemyInterface
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CharacterClassDefaults")
	int32 Level = 1;

	virtual void BeginPlay() override;

	virtual void InitAbilityActorInfo() override;

public:

	AAuraEnemy();

	void HighLightActor() override;

	void UnHighLightActor() override;

	/*
	* combat interface inherit from parent
	*/

	virtual int32 GetPlayerLevel() override;

};

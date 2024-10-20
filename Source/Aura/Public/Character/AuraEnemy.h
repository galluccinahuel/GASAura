
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

	virtual void BeginPlay() override;

public:

	AAuraEnemy();

	void HighLightActor() override;

	void UnHighLightActor() override;

};

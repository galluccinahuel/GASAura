
#pragma once

#include "CoreMinimal.h"
#include "Character/AuraCharacterBase.h"
#include "Interfaces/EnemyInterface.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "AuraEnemy.generated.h"

class UWidgetComponent;
class UBehaviorTree;
class AAuraAIController;

UCLASS()
class AURA_API AAuraEnemy : public AAuraCharacterBase, public IEnemyInterface
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CharacterClassDefaults")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CharacterClassDefaults")
	ECharacterClass CharacterClass = ECharacterClass::Ranger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(EditAnywhere, Category="AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY()
	TObjectPtr<AAuraAIController> AuraAIController;

	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual void InitilizeDefaultAttributes() const override;

public:

	AAuraEnemy();

	virtual void PossessedBy(AController* NewController) override;

	virtual void Die() override;

	UPROPERTY(BlueprintReadOnly, category = "Combat")
	bool bHitReacting = false;

	UPROPERTY(BlueprintReadOnly, category = "Combat")
	float BaseWalkSpeed = 250.f;

	UPROPERTY(EditAnywhere ,BlueprintReadOnly, category = "Combat")
	float LifeSpan = 250.f;
	
	void HighLightActor() override;

	void UnHighLightActor() override;

	/*
	* combat interface inherit from parent
	*/

	virtual int32 GetPlayerLevel() override;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeSignature OnMaxHealthChanged;

	void HitReactTagChange(const FGameplayTag CallbackTag, int32 NewCount);

};

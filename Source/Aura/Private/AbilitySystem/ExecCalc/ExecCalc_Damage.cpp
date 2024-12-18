
#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "AuraAbilityTypes.h"
#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Interfaces/CombatInterface.h"

struct AuraDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor)
	DECLARE_ATTRIBUTE_CAPTUREDEF(BlockChance)
	DECLARE_ATTRIBUTE_CAPTUREDEF(ArmorPenetration)
	DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalHitChance)
	DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalHitDamage)
	DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalHitResistance)
	
	AuraDamageStatics()
	{
		// Captura el atributo "Armor" de la clase UAuraAttributeSet para usarlo en cálculos de efectos.
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, Armor, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, BlockChance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, ArmorPenetration, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, CriticalHitChance, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, CriticalHitDamage, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, CriticalHitResistance, Target, false);
	}
};

// Singleton que garantiza que solo haya una instancia de AuraDamageStatics en memoria.
static const AuraDamageStatics& DamageStatics()
{
	static AuraDamageStatics DStatics;
	return DStatics;
}

UExecCalc_Damage::UExecCalc_Damage()
{
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().BlockChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorPenetrationDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalHitChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalHitDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalHitResistanceDef);
}

void UExecCalc_Damage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// Obtener los componentes de Ability System del origen y el objetivo.
	const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	
	// Obtener los actores asociados a los componentes ASC.
	AActor* SourceAvatar = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetAvatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	
	ICombatInterface* SourceCombatInterface = Cast<ICombatInterface>(SourceAvatar);
	ICombatInterface* TargetCombatInterface = Cast<ICombatInterface>(TargetAvatar);
	
	// Especificación del efecto y tags de origen y objetivo.
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// Configuración de parámetros para la evaluación del atributo.
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;
	
	float Damage = 0.f;
	for (FGameplayTag DamageTypeTag : FAuraGameplayTags::Get().DamageTypes)
	{
		const float DamageTypeValue = Spec.GetSetByCallerMagnitude(DamageTypeTag);
		Damage += DamageTypeValue;
	}

	float TargetBlockChance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().BlockChanceDef, EvaluationParameters, TargetBlockChance);
	TargetBlockChance = FMath::Max<float>(TargetBlockChance, 0.f);

	float TargetArmor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvaluationParameters, TargetArmor);
	TargetArmor = FMath::Max<float>(TargetArmor, 0.f);

	float SourceArmorPenetration = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorPenetrationDef, EvaluationParameters, SourceArmorPenetration);
	SourceArmorPenetration = FMath::Max<float>(SourceArmorPenetration, 0.f);
	
	// Log inicial del daño base
	UE_LOG(LogTemp, Log, TEXT("Daño inicial: %f"), Damage);
	
	// Normalizar BlockChance al rango [0, 1] y decidir si el ataque es bloqueado
	const float NormalizedBlockChance = FMath::Clamp(TargetBlockChance / 100.f, 0.f, 1.f);
	const bool bIsBlocked = FMath::FRand() < NormalizedBlockChance;

	
	FGameplayEffectContextHandle ContextHandle = Spec.GetContext();
	//FGameplayEffectContext* EffectContext = ContextHandle.Get();
	//FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContext);

	// Modificar el daño en caso de bloqueo
	constexpr float BlockDamageReductionFactor = 0.5f; // Reducir al 50%
	Damage = bIsBlocked ? Damage * BlockDamageReductionFactor : Damage;
	UAuraAbilitySystemLibrary::SetIsBlockedHit(ContextHandle, bIsBlocked);

	// Log de probabilidad de bloqueo y resultado
	UE_LOG(LogTemp, Log, TEXT("Probabilidad de bloqueo (normalizada): %f, ¿Bloqueado?: %s"), 
		NormalizedBlockChance, bIsBlocked ? TEXT("Sí") : TEXT("No"));

	// Log de daño después de bloqueo
	UE_LOG(LogTemp, Log, TEXT("Daño después de bloqueo: %f"), Damage);
	
	const UCharacterClassInfo* CharacterClassInfo = UAuraAbilitySystemLibrary::GetCharacterClassInfo(SourceAvatar);
	
	const FRealCurve* ArmorPenetrationCurve = CharacterClassInfo->DamageCalculationCoefficients->FindCurve(FName("ArmorPenetration"), FString());
	const float ArmorPenetrationCoefficient = ArmorPenetrationCurve->Eval(SourceCombatInterface->GetPlayerLevel());
	
	//ArmorPenetration ignores a percentage of the targets armor.
	const float EffectiveArmor = TargetArmor * (100 - SourceArmorPenetration * ArmorPenetrationCoefficient) / 100.f;

	const FRealCurve* ArmorCoefficient = CharacterClassInfo->DamageCalculationCoefficients->FindCurve(FName("EffectiveArmor"), FString());
	const float EffectiveArmorCoefficient = ArmorCoefficient->Eval(TargetCombatInterface->GetPlayerLevel());
	
	//Armor Ignores a percentage of incoming damage.
	Damage*= (100 - EffectiveArmor * EffectiveArmorCoefficient) / 100.f;
	
	float CriticalHitChance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalHitChanceDef, EvaluationParameters, CriticalHitChance);
	CriticalHitChance = FMath::Max<float>(CriticalHitChance, 0.f);

	float CriticalHitDamage = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalHitDamageDef, EvaluationParameters, CriticalHitDamage);
	CriticalHitDamage = FMath::Max<float>(CriticalHitDamage, 0.f);

	float CriticalHitResistance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalHitResistanceDef, EvaluationParameters, CriticalHitResistance);
	CriticalHitResistance = FMath::Max<float>(CriticalHitResistance, 0.f);

	const FRealCurve* CriticalHitResistanceCoefficient = CharacterClassInfo->DamageCalculationCoefficients->FindCurve(FName("CriticalHitResistance"), FString());
	const float EffectiveCriticalHitResistanceCoefficient = ArmorCoefficient->Eval(TargetCombatInterface->GetPlayerLevel());
	

	const float NormalizedCriticalHitChance = FMath::Clamp(CriticalHitChance / 100.f, 0.f, 1.f);
	const float NormalizedCriticalHitResistance = FMath::Clamp(CriticalHitResistance / 100.f, 0.f, 1.f);
	const bool bIsCritical = FMath::FRand() *  EffectiveCriticalHitResistanceCoefficient + NormalizedCriticalHitResistance < NormalizedCriticalHitChance;
	constexpr  float CriticalHitDamageFactor = 2.f;
	Damage = bIsCritical ? Damage * CriticalHitDamageFactor + CriticalHitDamage : Damage ;
	UAuraAbilitySystemLibrary::SetIsCriticalHit(ContextHandle,bIsCritical);

	
	// Log de armadura y penetración
	UE_LOG(LogTemp, Log, TEXT("Armadura efectiva: %f, Penetración de armadura: %f, Coeficiente: %f"), 
		EffectiveArmor, SourceArmorPenetration, ArmorPenetrationCoefficient);

	// Log de daño tras mitigación de armadura
	UE_LOG(LogTemp, Log, TEXT("Daño después de mitigación por armadura: %f"), Damage);

	// Log de probabilidad crítica y resultado
	UE_LOG(LogTemp, Log, TEXT("Probabilidad crítica (normalizada): %f, Resistencia crítica (normalizada): %f, ¿Crítico?: %s"), 
		NormalizedCriticalHitChance, NormalizedCriticalHitResistance, bIsCritical ? TEXT("Sí") : TEXT("No"));

	// Log de daño final
	UE_LOG(LogTemp, Log, TEXT("Daño final: %f"), Damage);
	
	// Crear y añadir el modificador evaluado al output
	const FGameplayModifierEvaluatedData EvaluatedData(UAuraAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage);
	OutExecutionOutput.AddOutputModifier(EvaluatedData); 
}

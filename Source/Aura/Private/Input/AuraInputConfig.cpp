
#include "Input/AuraInputConfig.h"

const UInputAction* UAuraInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FAuraInputAction& Action : AbilityInputActions)
	{
		if (Action.InputTag.MatchesTag(InputTag))
		{
			return Action.InputAction;
		}
		

	}
	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Can´t find abilityinputaction for input tag [%s], on inputconfig [%s]"), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

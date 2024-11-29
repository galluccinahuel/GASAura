#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"

// Implementación de una tarea personalizada para obtener datos del cursor del jugador.

UTargetDataUnderMouse* UTargetDataUnderMouse::CreateTargetDataUnderMouse(UGameplayAbility* OwningAbility)
{
	// Utilizamos NewAbilityTask para crear una nueva instancia de esta tarea específica.
	// Esto asegura que la tarea esté correctamente gestionada por el sistema de GAS.
	UTargetDataUnderMouse* MyObj = NewAbilityTask<UTargetDataUnderMouse>(OwningAbility);
	return MyObj;
}

void UTargetDataUnderMouse::Activate()
{
	// Obtenemos el PlayerController asociado al Ability actual.
	// Esto nos permite interactuar con los controles y el estado del jugador.
	APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();

	// Declaramos un objeto FHitResult para almacenar el resultado del trazado de colisión.
	FHitResult CursorHit;

	// Realizamos un trazado de colisión bajo el cursor del mouse, utilizando el canal de colisión ECC_Visibility.
	// Este método llena el FHitResult con información sobre lo que se encuentra debajo del cursor.
	PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);

	// Transmitimos la ubicación del impacto a través del delegado ValidData.
	// Esto permite que otros sistemas (suscriptores) reaccionen a esta información.
	ValidData.Broadcast(CursorHit.Location);
}
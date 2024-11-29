
#include "PlayerController/AuraPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Interfaces/EnemyInterface.h"
#include "Input/AuraInputComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "Components/SplineComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

AAuraPlayerController::AAuraPlayerController()
{
	bReplicates = true;
	Spline = CreateDefaultSubobject<USplineComponent>("Spline");
}

void AAuraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	CursorTrace();
	AutoRun();
}

void AAuraPlayerController::AutoRun()
{
	if (!bAutoRunning) return;

	// Obtenemos el Pawn controlado por este PlayerController
	if (APawn* ControlledPawn = GetPawn())
	{
		// Encontramos la ubicación en el Spline que está más cerca de la posición actual del personaje
		// Esto nos ayuda a mantener al personaje alineado con el Spline mientras se mueve
		const FVector LocationOnSpline = Spline->FindLocationClosestToWorldLocation(
			ControlledPawn->GetActorLocation(), ESplineCoordinateSpace::World);

		// Calculamos la dirección del movimiento basada en el punto más cercano en el Spline
		// Esto nos da un vector que apunta hacia adelante a lo largo del Spline
		const FVector Direction = Spline->FindDirectionClosestToWorldLocation(
			LocationOnSpline, ESplineCoordinateSpace::World);

		// Aplicamos la dirección calculada como input de movimiento para el personaje
		// Esto hará que el personaje se mueva en la dirección especificada
		ControlledPawn->AddMovementInput(Direction);

		// Calculamos la distancia entre la ubicación actual en el Spline y el destino final
		const float DistanceToDestination = (LocationOnSpline - CachedDestination).Length();

		// Si la distancia al destino es menor o igual al radio de aceptación (AutoRunAcceptanceRadius),
		// consideramos que hemos llegado al destino y desactivamos el auto-run
		if (DistanceToDestination <= AutoRunAcceptanceRadius)
		{
			bAutoRunning = false; // Detenemos el movimiento automático
		}
	}
}


void AAuraPlayerController::CursorTrace()
{
	
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, CursorHit);
	if (!CursorHit.bBlockingHit) return;

	LastActor = ThisActor;
	ThisActor = CursorHit.GetActor();

	/**
 * Line trace from cursor. There are several scenarios:
 *  A. LastActor is null && ThisActor is null
 *		- Do nothing
 *	B. LastActor is null && ThisActor is valid
 *		- Highlight ThisActor
 *	C. LastActor is valid && ThisActor is null
 *		- UnHighlight LastActor
 *	D. Both actors are valid, but LastActor != ThisActor
 *		- UnHighlight LastActor, and Highlight ThisActor
 *	E. Both actors are valid, and are the same actor
 *		- Do nothing
 */
	
	if (LastActor != ThisActor)
	{
		if (LastActor) LastActor->UnHighLightActor();
		if (ThisActor) ThisActor->HighLightActor();
	}
}

void AAuraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(AuraContext);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(AuraContext, 0);

	}

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}

void AAuraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UAuraInputComponent* AuraInputComponent = CastChecked<UAuraInputComponent>(InputComponent);
	AuraInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAuraPlayerController::Move);
	AuraInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void AAuraPlayerController::Move(const FInputActionValue& InputActionValue)
{
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}


void AAuraPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	//GEngine->AddOnScreenDebugMessage(1, 3.f, FColor::Cyan, *InputTag.ToString());

	if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{

		bTargeting = ThisActor ? true : false;
		bAutoRunning = false;
	}

}

void AAuraPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{

	// Comprobamos si el InputTag recibido es diferente al botón derecho del ratón (Right Mouse Button - RMB)
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{
		// Si tenemos un AbilitySystemComponent (ASC), delegamos el control a él
		if (GetASC())
		{
			GetASC()->AbilityInputTagReleased(InputTag);
		}
		return; // Salimos de la función, ya que no es RMB
	}
	// Si hemos llegado aquí, el InputTag es exactamente el RMB
	if (bTargeting)
	{
		// Si estamos en modo de targeting (apuntando), procesamos la habilidad usando el ASC
		if (GetASC())
		{
			GetASC()->AbilityInputTagReleased(InputTag);
		}
	}
	else
	{
		// Obtenemos el Pawn que estamos controlando
		APawn* ControlledPawn = GetPawn();

		// Comprobamos si el tiempo que se ha mantenido presionado (FollowTime) es menor que el umbral de tiempo corto (ShortPressThreshold)
		if (FollowTime <= ShortPressThreshold && ControlledPawn)
		{
			// Intentamos encontrar una ruta de navegación desde la ubicación actual del personaje hasta el destino almacenado (CachedDestination)
			if (UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(
				this, ControlledPawn->GetActorLocation(), CachedDestination))
			{
				// Limpiamos los puntos anteriores en el Spline, si es que existen
				Spline->ClearSplinePoints();

				// Iteramos a través de todos los puntos en la ruta calculada
				for (const FVector& PointLocation : NavPath->PathPoints)
				{
					// Añadimos cada punto al Spline para crear una trayectoria visual
					Spline->AddSplinePoint(PointLocation, ESplineCoordinateSpace::World);

					// Dibujamos una esfera verde en cada punto para propósitos de depuración 
					DrawDebugSphere(GetWorld(), PointLocation, 8.f, 8.f, FColor::Green, false, 3.f);
				}

				CachedDestination = NavPath->PathPoints[NavPath->PathPoints.Num() -1 ];

				// Indicamos que estamos en modo de auto-corre (auto-running) activando la bandera
				bAutoRunning = true;
			}
		}

		// Reiniciamos el tiempo de seguimiento (FollowTime) a 0 ya que hemos completado la acción
		FollowTime = 0.f;

		// Desactivamos el modo de apuntado (targeting)
		bTargeting = false;
	}


}

void AAuraPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	// Comprobamos si el InputTag recibido es diferente al botón derecho del ratón (Right Mouse Button - RMB)
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{
		// Si tenemos un AbilitySystemComponent (ASC), delegamos el control a él
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}	
		return; // Salimos de la función, ya que no es RMB
	}

	// Si hemos llegado aquí, el InputTag es exactamente el RMB
	if (bTargeting)
	{
		// Si estamos en modo de targeting (apuntando), procesamos la habilidad usando el ASC
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}
	}
	else
	{
		// Si no estamos apuntando, iniciamos un movimiento hacia donde el jugador apunta con el ratón
		FollowTime += GetWorld()->GetDeltaSeconds();

		// Obtener la posición en el mundo debajo del cursor del ratón

		if (CursorHit.bBlockingHit)
		{
			CachedDestination = CursorHit.ImpactPoint; // Guardamos la posición del impacto en CachedDestination
		}

		// Mover al personaje controlado hacia la posición del cursor
		if (APawn* ControlledPawn = GetPawn())
		{
			// Calculamos la dirección hacia la CachedDestination
			const FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
			// Añadimos input de movimiento en esa dirección
			ControlledPawn->AddMovementInput(WorldDirection);
		}
	}
}

UAuraAbilitySystemComponent* AAuraPlayerController::GetASC()
{
	if (AuraAbilitySystemComponent == nullptr)
	{
		AuraAbilitySystemComponent = Cast<UAuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return AuraAbilitySystemComponent;
}



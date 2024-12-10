
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
#include "GameFramework/Character.h"
#include "Systems/MovieSceneComponentAttachmentSystem.h"
#include "UI/Widget/DamageTextComponent.h"

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

void AAuraPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
	if (IsValid(TargetCharacter) && DamageComponentTextClass)
	{
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageComponentTextClass);
		DamageText->RegisterComponent();
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DamageText->SetDamageText(DamageAmount);
	}
}

void AAuraPlayerController::AutoRun()
{
	if (!bAutoRunning) return;

	// Obtenemos el Pawn controlado por este PlayerController
	if (APawn* ControlledPawn = GetPawn())
	{
		// Encontramos la ubicaci�n en el Spline que est� m�s cerca de la posici�n actual del personaje
		// Esto nos ayuda a mantener al personaje alineado con el Spline mientras se mueve
		const FVector LocationOnSpline = Spline->FindLocationClosestToWorldLocation(
			ControlledPawn->GetActorLocation(), ESplineCoordinateSpace::World);

		// Calculamos la direcci�n del movimiento basada en el punto m�s cercano en el Spline
		// Esto nos da un vector que apunta hacia adelante a lo largo del Spline
		const FVector Direction = Spline->FindDirectionClosestToWorldLocation(
			LocationOnSpline, ESplineCoordinateSpace::World);

		// Aplicamos la direcci�n calculada como input de movimiento para el personaje
		// Esto har� que el personaje se mueva en la direcci�n especificada
		ControlledPawn->AddMovementInput(Direction);

		// Calculamos la distancia entre la ubicaci�n actual en el Spline y el destino final
		const float DistanceToDestination = (LocationOnSpline - CachedDestination).Length();

		// Si la distancia al destino es menor o igual al radio de aceptaci�n (AutoRunAcceptanceRadius),
		// consideramos que hemos llegado al destino y desactivamos el auto-run
		if (DistanceToDestination <= AutoRunAcceptanceRadius)
		{
			bAutoRunning = false; // Detenemos el movimiento autom�tico
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
	AuraInputComponent->BindAction(ShiftAction, ETriggerEvent::Started, this, &AAuraPlayerController::ShiftPressed);
	AuraInputComponent->BindAction(ShiftAction, ETriggerEvent::Completed, this, &AAuraPlayerController::ShiftReleased );
	
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

	// Comprobamos si el InputTag recibido es diferente al bot�n derecho del rat�n (Right Mouse Button - RMB)
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{
		// Si tenemos un AbilitySystemComponent (ASC), delegamos el control a �l
		if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
		return; // Salimos de la funci�n, ya que no es RMB
	}

	if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
	
	// Si hemos llegado aqu�, el InputTag es exactamente el RMB
	if (!bTargeting && !bShiftKeyDown)
	{
		// Si estamos en modo de targeting (apuntando), procesamos la habilidad usando el ASC
		if (GetASC())
		{
			// Obtenemos el Pawn que estamos controlando
			const APawn* ControlledPawn = GetPawn();

			// Comprobamos si el tiempo que se ha mantenido presionado (FollowTime) es menor que el umbral de tiempo corto (ShortPressThreshold)
			if (FollowTime <= ShortPressThreshold && ControlledPawn)
			{
				// Intentamos encontrar una ruta de navegaci�n desde la ubicaci�n actual del personaje hasta el destino almacenado (CachedDestination)
				if (UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(
					this, ControlledPawn->GetActorLocation(), CachedDestination))
				{
					// Limpiamos los puntos anteriores en el Spline, si es que existen
					Spline->ClearSplinePoints();

					// Iteramos a trav�s de todos los puntos en la ruta calculada
					for (const FVector& PointLocation : NavPath->PathPoints)
					{
						// A�adimos cada punto al Spline para crear una trayectoria visual
						Spline->AddSplinePoint(PointLocation, ESplineCoordinateSpace::World);

						// Dibujamos una esfera verde en cada punto para prop�sitos de depuraci�n 
						DrawDebugSphere(GetWorld(), PointLocation, 8.f, 8.f, FColor::Green, false, 3.f);
					}

					CachedDestination = NavPath->PathPoints[NavPath->PathPoints.Num() -1 ];

					// Indicamos que estamos en modo de auto-corre (auto-running) activando la bandera
					bAutoRunning = true;
				}
			}

			// Reiniciamos el tiempo de seguimiento (FollowTime) a 0 ya que hemos completado la acci�n
			FollowTime = 0.f;

			// Desactivamos el modo de apuntado (targeting)
			bTargeting = false;
		}
	}
}

void AAuraPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	// Comprobamos si el InputTag recibido es diferente al bot�n derecho del rat�n (Right Mouse Button - RMB)
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{
		// Si tenemos un AbilitySystemComponent (ASC), delegamos el control a �l
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}	
		return; // Salimos de la funci�n, ya que no es RMB
	}

	// Si hemos llegado aqu�, el InputTag es exactamente el RMB
	if (bTargeting || bShiftKeyDown)
	{
		// Si estamos en modo de targeting (apuntando), procesamos la habilidad usando el ASC
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}
	}
	else
	{
		// Si no estamos apuntando, iniciamos un movimiento hacia donde el jugador apunta con el rat�n
		FollowTime += GetWorld()->GetDeltaSeconds();

		// Obtener la posici�n en el mundo debajo del cursor del rat�n

		if (CursorHit.bBlockingHit)
		{
			CachedDestination = CursorHit.ImpactPoint; // Guardamos la posici�n del impacto en CachedDestination
		}

		// Mover al personaje controlado hacia la posici�n del cursor
		if (APawn* ControlledPawn = GetPawn())
		{
			// Calculamos la direcci�n hacia la CachedDestination
			const FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
			// A�adimos input de movimiento en esa direcci�n
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



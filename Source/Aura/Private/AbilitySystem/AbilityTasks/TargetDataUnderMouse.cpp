#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemComponent.h"

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
    // Verifica si el personaje que está ejecutando la habilidad está controlado localmente (por el jugador)
    const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();

    // Si el personaje está controlado localmente (jugador), envia los datos del cursor
    if (bIsLocallyControlled)
    {
        SendMouseCursorData();  // Llama a la función que envía los datos del cursor
    }
    else
    {
        // En el servidor, se gestionan los datos del objetivo replicado
        // Obtiene el handle y la clave de predicción de la habilidad activa
        const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
        const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();

        // Añade una delegación para manejar la replicación de datos del objetivo
        AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UTargetDataUnderMouse::OnTargetDataReplicatedCallback);

        // Intenta llamar a los delegados de replicación de datos del objetivo si están configurados
        const bool bCalledDelegate = AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey);

        // Si no se llamaron los delegados, espera los datos del jugador remoto
        if (!bCalledDelegate)
        {
            SetWaitingOnRemotePlayerData();
        }
    }
}

void UTargetDataUnderMouse::SendMouseCursorData()
{
    // Crea una ventana de predicción (para predicción de habilidad en el cliente)
    FScopedPredictionWindow FScopedPrediction(AbilitySystemComponent.Get());

    // Obtiene el controlador del jugador (necesario para obtener la información del cursor)
    APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();

    // Declara una estructura FHitResult para almacenar la información del impacto del cursor
    FHitResult CursorHit;

    PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);

    // Crea un objeto de datos de objetivo, que es la información del objetivo que estamos apuntando
    FGameplayAbilityTargetDataHandle DataHandle;

    // Crea un nuevo dato de objetivo para un impacto único, lo configura con el FHitResult del cursor
    FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
    Data->HitResult = CursorHit;

    // Agrega los datos del impacto al "DataHandle"
    DataHandle.Add(Data);

    // Envia los datos del objetivo replicados al servidor, para que los clientes tengan acceso
    AbilitySystemComponent->ServerSetReplicatedTargetData(
        GetAbilitySpecHandle(),               // El "Handle" que identifica esta habilidad
        GetActivationPredictionKey(),         // La clave de predicción para la activación de habilidad
        DataHandle,                           // Los datos del objetivo (aquí es el impacto del cursor)
        FGameplayTag(),                       // Etiqueta asociada a la habilidad (no se usa aquí)
        AbilitySystemComponent->ScopedPredictionKey  // Clave de predicción del sistema de habilidades
    );

    // Si está configurado para hacerlo, envía un evento de Broadcast con los datos de objetivo
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);  // Difunde los datos válidos del objetivo
    }
}

void UTargetDataUnderMouse::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag)
{
    // Consume los datos del objetivo replicados en el cliente después de que han sido recibidos
    AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());

    // Si está configurado para hacerlo, difunde los datos del objetivo válidos
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);  // Difunde los datos válidos del objetivo
    }
}


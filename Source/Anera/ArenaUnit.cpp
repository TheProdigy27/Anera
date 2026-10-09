#include "ArenaUnit.h"
#include "ArenaAttackComponent.h"
#include "ArenaCardData.h"
#include "ArenaHealthComponent.h"
#include "ArenaUnitAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

AArenaUnit::AArenaUnit()
{
	HealthComponent = CreateDefaultSubobject<UArenaHealthComponent>(TEXT("Vie"));
	AttackComponent = CreateDefaultSubobject<UArenaAttackComponent>(TEXT("Attaque"));

	// Sur le serveur, chaque unité qui apparaît reçoit automatiquement son contrôleur IA.
	AIControllerClass = AArenaUnitAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// L'unité se tourne dans le sens de sa marche.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AArenaUnit::InitializeUnit(UArenaCardData* InCard, EArenaTeam InTeam)
{
	check(HasAuthority());
	Team = InTeam;
	Card = InCard;
	if (!Card)
	{
		return;
	}

	HealthComponent->SetMaxHealth(Card->MaxHealth);
	AttackComponent->SetStats(Card->Attack);
	GetCharacterMovement()->MaxWalkSpeed = Card->MoveSpeed;
}

void AArenaUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaUnit, Team);
	DOREPLIFETIME(AArenaUnit, Card);
}

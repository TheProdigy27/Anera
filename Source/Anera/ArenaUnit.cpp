#include "ArenaUnit.h"
#include "ArenaAttackComponent.h"
#include "ArenaCardData.h"
#include "ArenaHealthBarComponent.h"
#include "ArenaHealthComponent.h"
#include "ArenaUnitAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AArenaUnit::AArenaUnit()
{
	GetCapsuleComponent()->InitCapsuleSize(35.f, 60.f);
	// Les unités ne se bloquent pas physiquement entre elles (une unité arrêtée sur un pont barrerait le
	// passage pour de bon) : l'évitement RVO ci-dessous suffit à les écarter les unes des autres.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// Formes simples du moteur, référencées ici pour qu'elles soient incluses dans le jeu packagé.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Le visuel est attaché au Mesh du Character, pas à la capsule. Chez un client, la capsule est recalée d'un
	// coup à chaque position reçue du serveur, et CharacterMovement ne lisse que le Mesh (il le décale puis le
	// ramène en douceur sur la capsule). Le Mesh est vide pour l'instant : il recevra le modèle animé en phase 4.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Corps"));
	BodyMesh->SetupAttachment(GetMesh());
	BodyMesh->SetStaticMesh(CylinderMesh.Object);
	BodyMesh->SetMaterial(0, ShapeMaterial.Object);
	// Le cylindre du moteur fait 100 × 100 × 100 : on l'ajuste à la capsule (70 de large, 120 de haut).
	BodyMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.2f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetGenerateOverlapEvents(false);
	// Le surlignage de la zone de déploiement (un décal) colore le sol, pas les unités qui s'y trouvent.
	BodyMesh->SetReceivesDecals(false);

	HealthComponent = CreateDefaultSubobject<UArenaHealthComponent>(TEXT("Vie"));
	AttackComponent = CreateDefaultSubobject<UArenaAttackComponent>(TEXT("Attaque"));

	// Barre de PV juste au-dessus du cylindre (120 de haut, centré sur le Mesh).
	HealthBar = CreateDefaultSubobject<UArenaHealthBarComponent>(TEXT("BarrePV"));
	HealthBar->SetupAttachment(GetMesh());
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, 85.f));

	// Sur le serveur, chaque unité qui apparaît reçoit automatiquement son contrôleur IA.
	AIControllerClass = AArenaUnitAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// L'unité se tourne dans le sens de sa marche, et s'écarte des autres (évitement RVO).
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->bUseRVOAvoidance = true;
	Movement->AvoidanceConsiderationRadius = 120.f;
	Movement->BrakingDecelerationFlying = 2000.f;

	// Positions envoyées aux clients 30 fois par seconde (100 par défaut pour un Pawn) : entre deux envois, le
	// client prolonge le mouvement à la vitesse reçue, et le lissage gomme les écarts. Cela divise par trois
	// le débit, qui grandit avec le nombre d'unités en jeu.
	SetNetUpdateFrequency(30.f);
}

void AArenaUnit::InitializeUnit(UArenaCardData* InCard, EArenaTeam InTeam, float DeployDelay)
{
	check(HasAuthority());
	Team = InTeam;
	Card = InCard;
	DeployEndTime = GetWorld()->GetTimeSeconds() + DeployDelay;
	if (!Card)
	{
		return;
	}

	HealthComponent->SetMaxHealth(Card->MaxHealth);
	AttackComponent->SetStats(Card->Attack);
	BaseMoveSpeed = Card->MoveSpeed;
	UpdateMoveSpeed();

	// Évitement : chaque unité contourne ses alliés, mais pas ses ennemis, qu'elle doit pouvoir rejoindre.
	FNavAvoidanceMask OwnGroup;
	OwnGroup.SetGroup(Team == EArenaTeam::J ? 0 : 1);
	FNavAvoidanceMask EnemyGroup;
	EnemyGroup.SetGroup(Team == EArenaTeam::J ? 1 : 0);
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetAvoidanceGroupMask(OwnGroup);
	Movement->SetGroupsToAvoidMask(OwnGroup);
	Movement->SetGroupsToIgnoreMask(EnemyGroup);

	if (Card->bFlying)
	{
		// Une unité volante ignore la NavMesh : elle vole en ligne droite à altitude constante.
		Movement->DefaultLandMovementMode = MOVE_Flying;
		Movement->GetNavAgentPropertiesRef().bCanFly = true;
	}
}

float AArenaUnit::GetSpawnHeight(bool bFlyingUnit) const
{
	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + (bFlyingUnit ? FlyingHeight : 0.f) + 2.f;
}

bool AArenaUnit::IsFlying() const
{
	return Card && Card->bFlying;
}

float AArenaUnit::GetRadius() const
{
	return GetCapsuleComponent()->GetScaledCapsuleRadius();
}

FVector AArenaUnit::GetVisualLocation() const
{
	return BodyMesh->GetComponentLocation();
}

bool AArenaUnit::IsDeployed() const
{
	return GetWorld()->GetTimeSeconds() >= DeployEndTime;
}

void AArenaUnit::ApplySlow(float SlowAmount, float Duration)
{
	check(HasAuthority());
	CurrentSlow = FMath::Max(CurrentSlow, FMath::Clamp(SlowAmount, 0.f, 1.f));
	UpdateMoveSpeed();
	GetWorldTimerManager().SetTimer(SlowTimer, this, &AArenaUnit::ClearSlow, Duration, false);
}

void AArenaUnit::ClearSlow()
{
	CurrentSlow = 0.f;
	UpdateMoveSpeed();
}

void AArenaUnit::UpdateMoveSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = BaseMoveSpeed * (1.f - CurrentSlow);
	Movement->MaxFlySpeed = Movement->MaxWalkSpeed;
}

void AArenaUnit::BeginPlay()
{
	// Avant Super::BeginPlay, qui démarre les composants : la barre crée son widget avec cette classe.
	HealthBar->SetWidgetClass(HealthBarWidgetClass);
	Super::BeginPlay();
	UpdateTeamColor();

	if (HasAuthority())
	{
		HealthComponent->OnDied.AddDynamic(this, &AArenaUnit::HandleDied);

		if (IsFlying())
		{
			// Le vol se fait dans un plan horizontal, à l'altitude d'apparition.
			UCharacterMovementComponent* Movement = GetCharacterMovement();
			Movement->SetMovementMode(MOVE_Flying);
			Movement->SetPlaneConstraintEnabled(true);
			Movement->SetPlaneConstraintNormal(FVector::UpVector);
			Movement->SetPlaneConstraintOrigin(GetActorLocation());
		}
	}
}

void AArenaUnit::OnRep_Team()
{
	UpdateTeamColor();
}

void AArenaUnit::UpdateTeamColor()
{
	if (Team == EArenaTeam::None)
	{
		return;
	}
	if (!BodyMaterial)
	{
		BodyMaterial = BodyMesh->CreateDynamicMaterialInstance(0);
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Team == EArenaTeam::J ? TeamJColor : TeamAColor);
	}
}

void AArenaUnit::HandleDied(UArenaHealthComponent* DeadHealth, AActor* Killer)
{
	// La destruction est répliquée : l'unité disparaît aussi chez les clients.
	Destroy();
}

void AArenaUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaUnit, Team);
	DOREPLIFETIME(AArenaUnit, Card);
}

#include "ArenaBuilding.h"
#include "ArenaAttackComponent.h"
#include "ArenaHealthComponent.h"
#include "Net/UnrealNetwork.h"

AArenaBuilding::AArenaBuilding()
{
	bReplicates = true;
	// Les bâtiments comptent pour toute la partie : toujours envoyés aux deux joueurs, où qu'ils regardent.
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	HealthComponent = CreateDefaultSubobject<UArenaHealthComponent>(TEXT("Vie"));
	AttackComponent = CreateDefaultSubobject<UArenaAttackComponent>(TEXT("Attaque"));

	TowerAttack.Damage = 50.f;
	TowerAttack.Interval = 0.8f;
	TowerAttack.Range = 700.f;
	TowerAttack.TargetType = EArenaTargetType::GroundAndAir;

	FortAttack.Damage = 70.f;
	FortAttack.Interval = 1.f;
	FortAttack.Range = 700.f;
	FortAttack.TargetType = EArenaTargetType::GroundAndAir;
}

void AArenaBuilding::InitializeBuilding(FName InGroup, EArenaBuildingType InType, EArenaTeam InTeam)
{
	check(HasAuthority());
	Group = InGroup;
	BuildingType = InType;
	Team = InTeam;

	const bool bIsFort = BuildingType == EArenaBuildingType::Fort;
	HealthComponent->SetMaxHealth(bIsFort ? FortMaxHealth : TowerMaxHealth);
	AttackComponent->SetStats(bIsFort ? FortAttack : TowerAttack);
}

float AArenaBuilding::GetDistanceToWalls(const FVector& Point) const
{
	return FMath::Max(0.f, static_cast<float>(FVector::Dist2D(Point, GetActorLocation())) - FallbackRadius);
}

FVector AArenaBuilding::GetClosestWallPoint(const FVector& Point) const
{
	const FVector Center = GetActorLocation();
	return Center + (Point - Center).GetSafeNormal2D() * FallbackRadius;
}

FVector AArenaBuilding::GetAimPoint() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 150.f);
}

void AArenaBuilding::OnRep_BuildingState()
{
}

void AArenaBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaBuilding, Group);
	DOREPLIFETIME(AArenaBuilding, BuildingType);
	DOREPLIFETIME(AArenaBuilding, Team);
	DOREPLIFETIME(AArenaBuilding, BuildingState);
}

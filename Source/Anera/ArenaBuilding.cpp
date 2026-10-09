#include "ArenaBuilding.h"
#include "Anera.h"
#include "ArenaAttackComponent.h"
#include "ArenaCombat.h"
#include "ArenaGameState.h"
#include "ArenaHealthComponent.h"
#include "ArenaLevel.h"
#include "ArenaProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AArenaBuilding::AArenaBuilding()
{
	PrimaryActorTick.bCanEverTick = true;
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
	TowerAttack.ProjectileClass = AArenaProjectile::StaticClass();

	FortAttack.Damage = 70.f;
	FortAttack.Interval = 1.f;
	FortAttack.Range = 700.f;
	FortAttack.TargetType = EArenaTargetType::GroundAndAir;
	FortAttack.ProjectileClass = AArenaProjectile::StaticClass();
}

void AArenaBuilding::InitializeBuilding(FName InGroup, EArenaBuildingType InType, EArenaTeam InTeam, const FVector& ProjectileOrigin)
{
	check(HasAuthority());
	Group = InGroup;
	BuildingType = InType;
	Team = InTeam;

	const bool bIsFort = BuildingType == EArenaBuildingType::Fort;
	bActive = !bIsFort;
	HealthComponent->SetMaxHealth(bIsFort ? FortMaxHealth : TowerMaxHealth);
	AttackComponent->SetStats(bIsFort ? FortAttack : TowerAttack);
	AttackComponent->SetProjectileOrigin(ProjectileOrigin);
}

void AArenaBuilding::BeginPlay()
{
	Super::BeginPlay();

	GatherLevelActors();
	ApplyBuildingState();

	// Ciblage et réactions aux dégâts : sur le serveur seulement.
	SetActorTickEnabled(HasAuthority());
	if (HasAuthority())
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &AArenaBuilding::HandleHealthChanged);
		HealthComponent->OnDamaged.AddDynamic(this, &AArenaBuilding::HandleDamaged);
		HealthComponent->OnDied.AddDynamic(this, &AArenaBuilding::HandleDied);
	}
}

void AArenaBuilding::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AArenaGameState* GameState = GetWorld()->GetGameState<AArenaGameState>();
	const bool bCanShoot = bActive && !IsDestroyed() && GameState && GameState->IsMatchRunning();
	AttackComponent->SetAttackEnabled(bCanShoot);
	if (!bCanShoot)
	{
		AttackComponent->SetTarget(nullptr);
		return;
	}

	// Une cible à portée est gardée ; sinon on cherche l'unité ennemie la plus proche, quelques fois par seconde.
	if (AttackComponent->IsTargetInRange() && ArenaCombat::IsAlive(AttackComponent->GetTarget()))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextTargetSearchTime)
	{
		return;
	}
	NextTargetSearchTime = Now + TargetSearchInterval;
	const FArenaAttackStats Stats = AttackComponent->GetStats();
	AttackComponent->SetTarget(ArenaCombat::FindNearestEnemy(this, Stats.TargetType, Stats.Range, false));
}

void AArenaBuilding::Activate()
{
	check(HasAuthority());
	if (!bActive && !IsDestroyed())
	{
		bActive = true;
		UE_LOG(LogArene, Log, TEXT("%s se reveille"), *Group.ToString());
	}
}

void AArenaBuilding::GatherLevelActors()
{
	if (bGathered || Group.IsNone())
	{
		return;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, FName(*(TEXT("batiment:") + Group.ToString())), Found);
	if (Found.IsEmpty())
	{
		UE_LOG(LogArene, Warning, TEXT("Aucun acteur avec le tag 'batiment:%s' : le batiment restera invisible."), *Group.ToString());
	}

	FBox IntactBounds(ForceInit);
	for (AActor* Actor : Found)
	{
		const FString State = ArenaLevel::GetTagValue(Actor, TEXT("etat"));
		const int32 Index = State == TEXT("Intact") ? 0 : State == TEXT("Endommage") ? 1 : State == TEXT("Ruine") ? 2 : INDEX_NONE;
		if (Index == INDEX_NONE)
		{
			continue;
		}
		StateActors[Index].Add(Actor);
		if (Index == 0 && Actor->IsA<AStaticMeshActor>())
		{
			IntactBounds += Actor->GetComponentsBoundingBox(true);
		}
	}

	if (IntactBounds.IsValid)
	{
		// Les projectiles visent le bas du bâtiment intact ; son rayon sert si les collisions manquent.
		const FVector Center = IntactBounds.GetCenter();
		const FVector Extent = IntactBounds.GetExtent();
		AimPoint = FVector(Center.X, Center.Y, IntactBounds.Min.Z + 0.4f * IntactBounds.GetSize().Z);
		FallbackRadius = FMath::Min(Extent.X, Extent.Y);
	}
	else
	{
		AimPoint = GetActorLocation() + FVector(0.f, 0.f, 150.f);
	}
	bGathered = true;
}

void AArenaBuilding::ApplyBuildingState()
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(StateActors); ++Index)
	{
		const bool bVisible = Index == static_cast<int32>(BuildingState);
		for (const TWeakObjectPtr<AActor>& Weak : StateActors[Index])
		{
			if (AActor* Actor = Weak.Get())
			{
				Actor->SetActorHiddenInGame(!bVisible);
				Actor->SetActorEnableCollision(bVisible);
			}
		}
	}
}

void AArenaBuilding::ForEachWallComponent(TFunctionRef<void(const UPrimitiveComponent*)> Visit) const
{
	for (const TWeakObjectPtr<AActor>& Weak : StateActors[static_cast<int32>(BuildingState)])
	{
		const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Weak.Get());
		const UStaticMeshComponent* Component = MeshActor ? MeshActor->GetStaticMeshComponent() : nullptr;
		if (Component && Component->IsCollisionEnabled())
		{
			Visit(Component);
		}
	}
}

float AArenaBuilding::GetDistanceToWalls(const FVector& Point) const
{
	float Best = TNumericLimits<float>::Max();
	ForEachWallComponent([&](const UPrimitiveComponent* Component)
	{
		FVector Closest;
		// -1 si la collision n'est pas disponible ; 0 si le point est à l'intérieur.
		if (Component->GetDistanceToCollision(Point, Closest) >= 0.f)
		{
			Best = FMath::Min(Best, static_cast<float>(FVector::Dist2D(Point, Closest)));
		}
	});
	if (Best < TNumericLimits<float>::Max())
	{
		return Best;
	}
	return FMath::Max(0.f, static_cast<float>(FVector::Dist2D(Point, GetActorLocation())) - FallbackRadius);
}

FVector AArenaBuilding::GetClosestWallPoint(const FVector& Point) const
{
	float BestDistance = TNumericLimits<float>::Max();
	FVector BestPoint = FVector::ZeroVector;
	ForEachWallComponent([&](const UPrimitiveComponent* Component)
	{
		FVector Closest;
		const float Distance = Component->GetDistanceToCollision(Point, Closest);
		if (Distance >= 0.f && Distance < BestDistance)
		{
			BestDistance = Distance;
			BestPoint = Closest;
		}
	});
	if (BestDistance < TNumericLimits<float>::Max())
	{
		return FVector(BestPoint.X, BestPoint.Y, Point.Z);
	}
	const FVector Center = GetActorLocation();
	return Center + (Point - Center).GetSafeNormal2D() * FallbackRadius;
}

void AArenaBuilding::HandleHealthChanged(UArenaHealthComponent* ChangedHealth, float Damage, AActor* DamageCauser)
{
	const float Fraction = HealthComponent->GetHealthFraction();
	const EArenaBuildingState NewState = Fraction <= 0.f ? EArenaBuildingState::Ruined
		: Fraction > DamagedThreshold ? EArenaBuildingState::Intact
		: EArenaBuildingState::Damaged;
	if (NewState != BuildingState)
	{
		BuildingState = NewState;
		ApplyBuildingState();
	}
}

void AArenaBuilding::HandleDamaged(UArenaHealthComponent* DamagedHealth, float Damage, AActor* DamageCauser)
{
	Activate();
}

void AArenaBuilding::HandleDied(UArenaHealthComponent* DeadHealth, AActor* Killer)
{
	BuildingState = EArenaBuildingState::Ruined;
	ApplyBuildingState();
	AttackComponent->SetAttackEnabled(false);
	AttackComponent->SetTarget(nullptr);
	UE_LOG(LogArene, Log, TEXT("%s detruit"), *Group.ToString());
	OnBuildingDestroyed.Broadcast(this);
}

void AArenaBuilding::OnRep_Group()
{
	GatherLevelActors();
	ApplyBuildingState();
}

void AArenaBuilding::OnRep_BuildingState()
{
	ApplyBuildingState();
	if (IsDestroyed())
	{
		OnBuildingDestroyed.Broadcast(this);
	}
}

void AArenaBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaBuilding, Group);
	DOREPLIFETIME(AArenaBuilding, BuildingType);
	DOREPLIFETIME(AArenaBuilding, Team);
	DOREPLIFETIME(AArenaBuilding, bActive);
	DOREPLIFETIME(AArenaBuilding, BuildingState);
}

#include "ArenaBuilding.h"
#include "Anera.h"
#include "ArenaAttackComponent.h"
#include "ArenaCombat.h"
#include "ArenaGameState.h"
#include "ArenaHealthBarComponent.h"
#include "ArenaHealthComponent.h"
#include "ArenaLevel.h"
#include "ArenaProjectile.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Largeur du socle non marchable autour des bâtiments importés (mesurée dans le niveau : environ 80 uu).
	constexpr double SocleWidth = 100.0;
}

AArenaBuilding::AArenaBuilding()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// Les bâtiments comptent pour toute la partie : toujours envoyés aux deux joueurs, où qu'ils regardent.
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	HealthComponent = CreateDefaultSubobject<UArenaHealthComponent>(TEXT("Vie"));
	AttackComponent = CreateDefaultSubobject<UArenaAttackComponent>(TEXT("Attaque"));
	HealthBar = CreateDefaultSubobject<UArenaHealthBarComponent>(TEXT("BarrePV"));
	HealthBar->SetupAttachment(RootComponent);

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
	// Avant Super::BeginPlay, qui démarre les composants : la barre crée son widget avec cette classe.
	HealthBar->SetWidgetClass(HealthBarWidgetClass);
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
			MeshBounds += Actor->GetComponentsBoundingBox(true);
		}
	}

	if (MeshBounds.IsValid)
	{
		// Emprise au sol du bâtiment intact (pour les distances), et point de repère dans le bas du bâtiment.
		// Le socle autour du bâtiment n'est pas marchable et ne fait pas partie de son mesh : l'emprise est
		// élargie d'autant, sans quoi une unité de mêlée resterait hors de portée au bord du socle.
		const FVector Center = MeshBounds.GetCenter();
		Footprint = FBox2D(FVector2D(MeshBounds.Min), FVector2D(MeshBounds.Max)).ExpandBy(SocleWidth);
		AimPoint = FVector(Center.X, Center.Y, MeshBounds.Min.Z + 0.4f * MeshBounds.GetSize().Z);
	}
	else
	{
		AimPoint = GetActorLocation() + FVector(0.f, 0.f, 150.f);
	}

	// Barre de PV au-dessus du toit du bâtiment intact, à la verticale de son centre.
	const float BarZ = MeshBounds.IsValid ? MeshBounds.Max.Z + HealthBarHeight : AimPoint.Z + 250.f;
	HealthBar->SetWorldLocation(FVector(AimPoint.X, AimPoint.Y, BarZ));
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

float AArenaBuilding::GetDistanceToWalls(const FVector& Point) const
{
	return static_cast<float>(FVector::Dist2D(Point, GetClosestWallPoint(Point)));
}

FVector AArenaBuilding::GetClosestWallPoint(const FVector& Point) const
{
	// Le bâtiment est assimilé à son emprise au sol : un rectangle (ou un cercle autour du slot à défaut).
	// C'est un calcul simple, identique sur le serveur et les clients, sans requête physique.
	if (Footprint.bIsValid)
	{
		return FVector(FMath::Clamp(Point.X, Footprint.Min.X, Footprint.Max.X), FMath::Clamp(Point.Y, Footprint.Min.Y, Footprint.Max.Y), Point.Z);
	}
	const FVector Center = GetActorLocation();
	const FVector ToPoint = FVector(Point.X - Center.X, Point.Y - Center.Y, 0.f);
	if (ToPoint.Size() <= FallbackRadius)
	{
		return Point;
	}
	return FVector(Center.X, Center.Y, Point.Z) + ToPoint.GetSafeNormal() * FallbackRadius;
}

FVector AArenaBuilding::GetSurfacePoint(const FVector& From) const
{
	// Boîte du mesh intact, sans le socle : le point touché est sur les murs, à la hauteur du tireur
	// (ramenée entre le pied et le sommet du bâtiment).
	return MeshBounds.IsValid ? MeshBounds.GetClosestPointTo(From) : GetClosestWallPoint(From);
}

TArray<FVector> AArenaBuilding::GetPointsAroundWalls(float Offset, float Spacing) const
{
	TArray<FVector> Points;
	const double Z = GetActorLocation().Z;
	Spacing = FMath::Max(Spacing, 10.f);

	if (!Footprint.bIsValid)
	{
		// Bâtiment sans mesure : un cercle autour du slot.
		const double Radius = FallbackRadius + Offset;
		const int32 Count = FMath::Max(8, FMath::CeilToInt(2.0 * UE_DOUBLE_PI * Radius / Spacing));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * Index / Count;
			Points.Emplace(GetActorLocation().X + Radius * FMath::Cos(Angle), GetActorLocation().Y + Radius * FMath::Sin(Angle), Z);
		}
		return Points;
	}

	// Tour du rectangle de l'emprise élargi d'Offset, côté par côté.
	const FBox2D Box = Footprint.ExpandBy(Offset);
	const FVector2D Corners[] = { Box.Min, FVector2D(Box.Max.X, Box.Min.Y), Box.Max, FVector2D(Box.Min.X, Box.Max.Y) };
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const FVector2D Start = Corners[Side];
		const FVector2D End = Corners[(Side + 1) % 4];
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(Start, End) / Spacing));
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const FVector2D Point = FMath::Lerp(Start, End, static_cast<double>(Step) / Steps);
			Points.Emplace(Point.X, Point.Y, Z);
		}
	}
	return Points;
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

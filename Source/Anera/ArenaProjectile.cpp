#include "ArenaProjectile.h"
#include "ArenaCombat.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AArenaProjectile::AArenaProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// La position n'est pas envoyée en continu : chaque machine la calcule à partir de la cible.
	SetReplicatingMovement(false);
	// Sécurité : un projectile perdu disparaît de lui-même.
	InitialLifeSpan = 5.f;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(10.f);
	// L'impact est détecté à l'arrivée sur la cible : pas de collision physique.
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = CollisionComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visuel"));
	VisualMesh->SetupAttachment(CollisionComponent);
	VisualMesh->SetStaticMesh(SphereMesh.Object);
	VisualMesh->SetMaterial(0, ShapeMaterial.Object);
	VisualMesh->SetRelativeScale3D(FVector(0.2f));
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCastShadow(false);
}

void AArenaProjectile::InitializeProjectile(AActor* InTarget, float InDamage, float InSplashRadius, EArenaTargetType InTargetType, EArenaTeam InTeam,
	UArenaCardData* InCard)
{
	check(HasAuthority());
	Target = InTarget;
	Damage = InDamage;
	SplashRadius = InSplashRadius;
	TargetType = InTargetType;
	Team = InTeam;
	Card = InCard;
}

void AArenaProjectile::BeginPlay()
{
	Super::BeginPlay();
	// Sans cible connue (cas d'un client qui la reçoit en retard), le projectile part tout droit.
	Destination = IsValid(Target) ? ArenaCombat::GetImpactPoint(Target, GetActorLocation()) : GetActorLocation() + GetActorForwardVector() * 1000.f;
}

void AArenaProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bArrived)
	{
		return;
	}

	if (IsValid(Target))
	{
		Destination = ArenaCombat::GetImpactPoint(Target, GetActorLocation());
	}

	const FVector ToDestination = Destination - GetActorLocation();
	const float Distance = ToDestination.Size();
	const float Step = Speed * DeltaSeconds;
	if (Distance > Step)
	{
		SetActorLocationAndRotation(GetActorLocation() + ToDestination / Distance * Step, ToDestination.Rotation());
		return;
	}

	SetActorLocation(Destination);
	bArrived = true;
	if (HasAuthority())
	{
		Impact();
	}
	else
	{
		// Le client attend la destruction envoyée par le serveur, sans afficher le projectile arrêté.
		SetActorHiddenInGame(true);
	}
}

void AArenaProjectile::Impact()
{
	ArenaCombat::PlayEffect(GetWorld(), EArenaEffectType::ProjectileImpact, Destination, SplashRadius, 0.f, Team, Card);
	if (SplashRadius > 0.f)
	{
		ArenaCombat::ApplyAreaDamage(GetWorld(), Destination, SplashRadius, Damage, Team, TargetType, 1.f, GetOwner());
	}
	else if (ArenaCombat::IsAlive(Target))
	{
		ArenaCombat::DealDamage(Target, Damage, GetOwner());
	}
	Destroy();
}

void AArenaProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaProjectile, Target);
}

#include "ArenaProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AArenaProjectile::AArenaProjectile()
{
	bReplicates = true;
	SetReplicatingMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(10.f);
	// L'impact sera détecté par la distance à la cible (1.2.5) : pas de collision physique.
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = CollisionComponent;

	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Mouvement"));
	MovementComponent->InitialSpeed = 1500.f;
	MovementComponent->MaxSpeed = 1500.f;
	MovementComponent->ProjectileGravityScale = 0.f;
	MovementComponent->bRotationFollowsVelocity = true;
}

void AArenaProjectile::InitializeProjectile(AActor* InTarget, float InDamage, float InSplashRadius, EArenaTeam InTeam)
{
	check(HasAuthority());
	Target = InTarget;
	Damage = InDamage;
	SplashRadius = InSplashRadius;
	Team = InTeam;
}

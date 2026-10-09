#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaTypes.h"
#include "ArenaProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

// Projectile simple (flèche, boule) qui file vers sa cible et inflige ses dégâts à l'impact.
// Créé par le serveur et répliqué : les clients ne font que le voir passer.
// Squelette : le vol et l'impact arrivent en 1.2.5.
UCLASS()
class ANERA_API AArenaProjectile : public AActor
{
	GENERATED_BODY()

public:
	AArenaProjectile();

	// Serveur uniquement, juste après l'apparition.
	void InitializeProjectile(AActor* InTarget, float InDamage, float InSplashRadius, EArenaTeam InTeam);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Arene|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Arene|Projectile")
	TObjectPtr<UProjectileMovementComponent> MovementComponent;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Projectile")
	TWeakObjectPtr<AActor> Target;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	float Damage = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	float SplashRadius = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	EArenaTeam Team = EArenaTeam::None;
};

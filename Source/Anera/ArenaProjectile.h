#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaTypes.h"
#include "ArenaProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// Projectile simple (flèche, boule) qui file vers sa cible et inflige ses dégâts à l'impact.
// Créé par le serveur et répliqué. Sa cible est répliquée aussi : chaque machine calcule elle-même
// sa trajectoire (fluide, sans envoyer sa position à chaque image), mais seul le serveur applique les dégâts.
UCLASS()
class ANERA_API AArenaProjectile : public AActor
{
	GENERATED_BODY()

public:
	AArenaProjectile();

	// Serveur uniquement, entre SpawnActorDeferred et FinishSpawning.
	void InitializeProjectile(AActor* InTarget, float InDamage, float InSplashRadius, EArenaTargetType InTargetType, EArenaTeam InTeam);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	// Serveur : dégâts au point d'arrivée, puis destruction.
	void Impact();

	UPROPERTY(VisibleAnywhere, Category = "Arene|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Arene|Projectile")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Projectile", meta = (ClampMin = "1", Units = "cm/s"))
	float Speed = 1500.f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene|Projectile")
	TObjectPtr<AActor> Target;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	float Damage = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	float SplashRadius = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	EArenaTargetType TargetType = EArenaTargetType::GroundAndAir;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	EArenaTeam Team = EArenaTeam::None;

private:
	// Dernière position connue de la cible : le projectile y termine sa course si la cible disparaît.
	FVector Destination = FVector::ZeroVector;
	bool bArrived = false;
};

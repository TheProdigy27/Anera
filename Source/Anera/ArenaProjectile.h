#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaTypes.h"
#include "ArenaProjectile.generated.h"

class UArenaCardData;
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

	// Serveur uniquement, entre SpawnActorDeferred et FinishSpawning. InCard : carte du tireur (vide pour un bâtiment).
	void InitializeProjectile(AActor* InTarget, float InDamage, float InSplashRadius, EArenaTargetType InTargetType, EArenaTeam InTeam,
		UArenaCardData* InCard);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	// Serveur : dégâts et effet d'impact au point d'arrivée, puis destruction.
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

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Projectile")
	TObjectPtr<UArenaCardData> Card;

private:
	// Point d'impact sur la cible, suivi à chaque image. Si la cible disparaît, le projectile termine
	// sa course au dernier point connu.
	FVector Destination = FVector::ZeroVector;
	bool bArrived = false;
};

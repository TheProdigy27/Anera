#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArenaTypes.h"
#include "ArenaAttackComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaAttackSignature, AActor*, Target);

// Attaque d'une unité ou d'un bâtiment : portée, cadence, dégâts, type de cible, projectile, dégâts de zone.
// Le propriétaire choisit la cible (IA de l'unité, bâtiment) ; ce composant tire dès qu'elle est à portée,
// au rythme de sa cadence. Serveur uniquement : les clients ne calculent jamais de dégâts.
UCLASS(ClassGroup = (Arene), meta = (BlueprintSpawnableComponent))
class ANERA_API UArenaAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaAttackComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Remplace les caractéristiques (lues dans la carte de l'unité ou réglées sur le bâtiment).
	UFUNCTION(BlueprintCallable, Category = "Arene|Attaque")
	void SetStats(const FArenaAttackStats& NewStats) { Stats = NewStats; }

	UFUNCTION(BlueprintPure, Category = "Arene|Attaque")
	FArenaAttackStats GetStats() const { return Stats; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arene|Attaque")
	void SetTarget(AActor* NewTarget) { Target = NewTarget; }

	UFUNCTION(BlueprintPure, Category = "Arene|Attaque")
	AActor* GetTarget() const { return Target.Get(); }

	// Candidate est-elle à portée, de bord à bord ?
	bool IsInRange(const AActor* Candidate) const;

	bool IsTargetInRange() const { return IsInRange(Target.Get()); }

	// Coupe ou rétablit les tirs (fort pas encore réveillé, bâtiment détruit).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arene|Attaque")
	void SetAttackEnabled(bool bEnabled) { bAttackEnabled = bEnabled; }

	// Point de départ des projectiles, dans le monde. Sans réglage : un peu au-dessus de l'acteur.
	void SetProjectileOrigin(const FVector& WorldLocation);

	// Serveur : à chaque coup porté (pour les animations et effets de la phase 4).
	UPROPERTY(BlueprintAssignable, Category = "Arene|Attaque")
	FArenaAttackSignature OnAttack;

protected:
	virtual void BeginPlay() override;

	void PerformAttack(AActor* AttackTarget);
	FVector GetProjectileOrigin() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|Attaque")
	FArenaAttackStats Stats;

	// Pointeur faible : il devient nul tout seul si la cible est détruite.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Attaque")
	TWeakObjectPtr<AActor> Target;

	UPROPERTY(VisibleInstanceOnly, Category = "Arene|Attaque")
	bool bAttackEnabled = true;

private:
	bool bHasProjectileOrigin = false;
	FVector ProjectileOrigin = FVector::ZeroVector;
	double NextAttackTime = 0.0;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArenaTypes.h"
#include "ArenaAttackComponent.generated.h"

// Attaque d'une unité ou d'un bâtiment : portée, cadence, dégâts, type de cible, projectile, dégâts de zone.
// Serveur uniquement : le choix de la cible et les dégâts ne sont jamais calculés chez les clients.
// Squelette : la boucle d'attaque arrive en 1.2.5 (unités) et 1.2.6 (bâtiments).
UCLASS(ClassGroup = (Arene), meta = (BlueprintSpawnableComponent))
class ANERA_API UArenaAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaAttackComponent();

	// Remplace les caractéristiques (lues dans la carte de l'unité ou réglées sur le bâtiment).
	UFUNCTION(BlueprintCallable, Category = "Arene|Attaque")
	void SetStats(const FArenaAttackStats& NewStats) { Stats = NewStats; }

	UFUNCTION(BlueprintPure, Category = "Arene|Attaque")
	FArenaAttackStats GetStats() const { return Stats; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arene|Attaque")
	void SetTarget(AActor* NewTarget) { Target = NewTarget; }

	UFUNCTION(BlueprintPure, Category = "Arene|Attaque")
	AActor* GetTarget() const { return Target.Get(); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|Attaque")
	FArenaAttackStats Stats;

	// Pointeur faible : il devient nul tout seul si la cible est détruite.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Attaque")
	TWeakObjectPtr<AActor> Target;
};

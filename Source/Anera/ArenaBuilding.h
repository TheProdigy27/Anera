#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaTypes.h"
#include "ArenaBuilding.generated.h"

class UArenaAttackComponent;
class UArenaHealthComponent;

// Tour ou fort. Le GameMode en fait apparaître un sur chaque SLOT_Tour_* / SLOT_Fort_*.
// Il n'a pas de mesh à lui : il pilote les 3 meshes importés (Intact, Endommage, Ruine) de son groupe.
// Squelette : pilotage des meshes en 1.4, tirs en 1.2.6.
UCLASS()
class ANERA_API AArenaBuilding : public AActor
{
	GENERATED_BODY()

public:
	AArenaBuilding();

	// Serveur uniquement, juste après l'apparition. Group = nom du groupe importé (ex. « Tour_Gauche_J »).
	void InitializeBuilding(FName InGroup, EArenaBuildingType InType, EArenaTeam InTeam);

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	EArenaTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	EArenaBuildingType GetBuildingType() const { return BuildingType; }

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	EArenaBuildingState GetBuildingState() const { return BuildingState; }

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	FName GetGroup() const { return Group; }

	UArenaHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UArenaAttackComponent* GetAttackComponent() const { return AttackComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arene|Batiment")
	TObjectPtr<UArenaHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arene|Batiment")
	TObjectPtr<UArenaAttackComponent> AttackComponent;

	// Caractéristiques par type de bâtiment (voir PLAN.md, 1.3).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Batiment|Tour", meta = (ClampMin = "1"))
	float TowerMaxHealth = 1400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Batiment|Tour")
	FArenaAttackStats TowerAttack;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Batiment|Fort", meta = (ClampMin = "1"))
	float FortMaxHealth = 2400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Batiment|Fort")
	FArenaAttackStats FortAttack;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	FName Group;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	EArenaBuildingType BuildingType = EArenaBuildingType::Tower;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	EArenaTeam Team = EArenaTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_BuildingState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	EArenaBuildingState BuildingState = EArenaBuildingState::Intact;

	// Chez les clients : affiche le mesh correspondant au nouvel état (1.4).
	UFUNCTION()
	void OnRep_BuildingState();
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaTypes.h"
#include "ArenaBuilding.generated.h"

class AArenaBuilding;
class UArenaAttackComponent;
class UArenaHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaBuildingDestroyedSignature, AArenaBuilding*, Building);

// Tour ou fort. Le GameMode en fait apparaître un sur chaque SLOT_Tour_* / SLOT_Fort_*.
// Il n'a pas de mesh à lui : il pilote les acteurs importés de son groupe (tags « batiment:<Groupe> »
// et « etat:Intact|Endommage|Ruine » : meshes, flammes et lumières). Seul l'état courant est visible et a
// une collision. L'état découle des PV : au-dessus de 50 % Intact, puis Endommagé, et Ruine à 0.
// Chaque machine affiche elle-même les bons acteurs, à partir du groupe et de l'état répliqués.
//
// Les tours tirent sur l'ennemi le plus proche à portée. Le fort reste inactif tant qu'il n'a pas été
// touché et qu'aucune tour de son camp n'est détruite.
UCLASS()
class ANERA_API AArenaBuilding : public AActor
{
	GENERATED_BODY()

public:
	AArenaBuilding();

	// Serveur uniquement, entre SpawnActorDeferred et FinishSpawning.
	// Group = nom du groupe importé (ex. « Tour_Gauche_J ») ; ProjectileOrigin = sommet (SLOT_Unite_<Groupe>).
	void InitializeBuilding(FName InGroup, EArenaBuildingType InType, EArenaTeam InTeam, const FVector& ProjectileOrigin);

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	EArenaTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	EArenaBuildingType GetBuildingType() const { return BuildingType; }

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	EArenaBuildingState GetBuildingState() const { return BuildingState; }

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	FName GetGroup() const { return Group; }

	// Faux pour un fort encore endormi.
	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Arene|Batiment")
	bool IsDestroyed() const { return BuildingState == EArenaBuildingState::Ruined; }

	// Serveur : réveille le bâtiment (le fort, au premier coup reçu ou à la chute d'une tour de son camp).
	void Activate();

	UArenaHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UArenaAttackComponent* GetAttackComponent() const { return AttackComponent; }

	// Distance à plat entre un point et les murs du bâtiment (0 si le point est dedans).
	float GetDistanceToWalls(const FVector& Point) const;

	// Point des murs le plus proche de Point (pour qu'une unité vienne s'y placer).
	FVector GetClosestWallPoint(const FVector& Point) const;

	// Points répartis tout autour du bâtiment, à Offset de ses murs et espacés d'environ Spacing, au niveau du sol.
	TArray<FVector> GetPointsAroundWalls(float Offset, float Spacing) const;

	// Point du bas du bâtiment, au centre (repère pour l'affichage des PV).
	FVector GetAimPoint() const { return AimPoint; }

	// Point de la surface du bâtiment intact le plus proche de From : là où un coup venu de From le touche.
	FVector GetSurfacePoint(const FVector& From) const;

	// Serveur et clients : le bâtiment vient d'être détruit (couronnes, effets de la phase 4).
	UPROPERTY(BlueprintAssignable, Category = "Arene|Batiment")
	FArenaBuildingDestroyedSignature OnBuildingDestroyed;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

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

	// Au-dessus de cette part de PV, le bâtiment est intact ; en dessous, endommagé.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Batiment", meta = (ClampMin = "0", ClampMax = "1"))
	float DamagedThreshold = 0.5f;

	// Temps entre deux recherches de cible.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Batiment", meta = (ClampMin = "0.05", Units = "s"))
	float TargetSearchInterval = 0.2f;

	UPROPERTY(ReplicatedUsing = OnRep_Group, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	FName Group;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	EArenaBuildingType BuildingType = EArenaBuildingType::Tower;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	EArenaTeam Team = EArenaTeam::None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	bool bActive = true;

	// ReplicatedUsing : chez les clients, OnRep_BuildingState affiche le nouvel état dès qu'il arrive.
	UPROPERTY(ReplicatedUsing = OnRep_BuildingState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Batiment")
	EArenaBuildingState BuildingState = EArenaBuildingState::Intact;

private:
	UFUNCTION()
	void OnRep_Group();

	UFUNCTION()
	void OnRep_BuildingState();

	// Retrouve les acteurs importés du groupe, rangés par état, et mesure le bâtiment intact.
	void GatherLevelActors();

	// Montre les acteurs de l'état courant (avec collision), cache les autres (sans collision).
	void ApplyBuildingState();

	UFUNCTION()
	void HandleHealthChanged(UArenaHealthComponent* ChangedHealth, float Damage, AActor* DamageCauser);

	UFUNCTION()
	void HandleDamaged(UArenaHealthComponent* DamagedHealth, float Damage, AActor* DamageCauser);

	UFUNCTION()
	void HandleDied(UArenaHealthComponent* DeadHealth, AActor* Killer);

	// Acteurs importés de chaque état (Intact, Endommagé, Ruine). Ils appartiennent au niveau.
	TArray<TWeakObjectPtr<AActor>> StateActors[3];
	bool bGathered = false;

	// Mesures du bâtiment intact : boîte englobante, emprise au sol (élargie du socle) et point visé.
	// À défaut, un cercle autour du slot.
	FBox MeshBounds = FBox(ForceInit);
	FBox2D Footprint = FBox2D(ForceInit);
	FVector AimPoint = FVector::ZeroVector;
	float FallbackRadius = 120.f;

	double NextTargetSearchTime = 0.0;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaTypes.h"
#include "ArenaUnit.generated.h"

class UArenaAttackComponent;
class UArenaCardData;
class UArenaHealthComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// Unité posée par une carte. ACharacter fournit la capsule de collision et le déplacement
// (CharacterMovement, répliqué et interpolé chez les clients). Elle est pilotée par un AArenaUnitAIController.
// En attendant les modèles (phase 4), elle s'affiche comme un cylindre à la couleur de son camp.
UCLASS()
class ANERA_API AArenaUnit : public ACharacter
{
	GENERATED_BODY()

public:
	AArenaUnit();

	// Serveur uniquement, entre SpawnActorDeferred et FinishSpawning : camp, caractéristiques de la carte
	// et délai avant que l'unité n'agisse.
	void InitializeUnit(UArenaCardData* InCard, EArenaTeam InTeam, float DeployDelay);

	// Hauteur entre le sol et le centre de l'unité à son apparition (plus haut pour une unité volante).
	float GetSpawnHeight(bool bFlyingUnit) const;

	UFUNCTION(BlueprintPure, Category = "Arene|Unite")
	EArenaTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Arene|Unite")
	UArenaCardData* GetCard() const { return Card; }

	UFUNCTION(BlueprintPure, Category = "Arene|Unite")
	bool IsFlying() const;

	// Rayon de la capsule : sert aux distances de bord à bord.
	float GetRadius() const;

	// Centre de l'unité telle qu'elle est affichée. Sur le serveur, c'est le centre de la capsule ; chez un
	// client, le visuel suit la capsule avec le lissage réseau, donc parfois quelques centimètres derrière.
	FVector GetVisualLocation() const;

	// Serveur : vrai une fois le délai d'apparition écoulé.
	bool IsDeployed() const;

	// Serveur : ralentit l'unité pendant Duration (SlowAmount = 0,5 : 50 % plus lente).
	// Si plusieurs ralentissements se cumulent, le plus fort l'emporte.
	void ApplySlow(float SlowAmount, float Duration);

	UArenaHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UArenaAttackComponent* GetAttackComponent() const { return AttackComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arene|Unite")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arene|Unite")
	TObjectPtr<UArenaHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arene|Unite")
	TObjectPtr<UArenaAttackComponent> AttackComponent;

	// Altitude des unités volantes au-dessus du sol.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Unite", meta = (ClampMin = "0", Units = "cm"))
	float FlyingHeight = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Unite")
	FLinearColor TeamJColor = FLinearColor(0.05f, 0.25f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Unite")
	FLinearColor TeamAColor = FLinearColor(1.f, 0.1f, 0.05f);

	UPROPERTY(ReplicatedUsing = OnRep_Team, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Unite")
	EArenaTeam Team = EArenaTeam::None;

	// Carte qui a fait apparaître l'unité (répliquée : les clients en tirent nom, icône, etc.).
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Unite")
	TObjectPtr<UArenaCardData> Card;

private:
	UFUNCTION()
	void OnRep_Team();

	void UpdateTeamColor();
	void UpdateMoveSpeed();
	void ClearSlow();

	UFUNCTION()
	void HandleDied(UArenaHealthComponent* DeadHealth, AActor* Killer);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	double DeployEndTime = 0.0;
	float BaseMoveSpeed = 0.f;
	float CurrentSlow = 0.f;
	FTimerHandle SlowTimer;
};

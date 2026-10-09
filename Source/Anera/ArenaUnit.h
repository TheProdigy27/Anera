#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaTypes.h"
#include "ArenaUnit.generated.h"

class UArenaAttackComponent;
class UArenaCardData;
class UArenaHealthComponent;

// Unité posée par une carte. ACharacter fournit la capsule de collision et le déplacement
// (CharacterMovement, répliqué et interpolé chez les clients). Elle est pilotée par un AArenaUnitAIController.
// Squelette : déplacement, ciblage et attaque arrivent en 1.2.5.
UCLASS()
class ANERA_API AArenaUnit : public ACharacter
{
	GENERATED_BODY()

public:
	AArenaUnit();

	// Serveur uniquement, juste après l'apparition : camp et caractéristiques lues dans la carte.
	void InitializeUnit(UArenaCardData* InCard, EArenaTeam InTeam);

	UFUNCTION(BlueprintPure, Category = "Arene|Unite")
	EArenaTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Arene|Unite")
	UArenaCardData* GetCard() const { return Card; }

	UArenaHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UArenaAttackComponent* GetAttackComponent() const { return AttackComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arene|Unite")
	TObjectPtr<UArenaHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arene|Unite")
	TObjectPtr<UArenaAttackComponent> AttackComponent;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Unite")
	EArenaTeam Team = EArenaTeam::None;

	// Carte qui a fait apparaître l'unité (répliquée : les clients en tirent nom, icône, etc.).
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Unite")
	TObjectPtr<UArenaCardData> Card;
};

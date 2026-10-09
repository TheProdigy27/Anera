#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArenaTypes.h"
#include "ArenaCardData.generated.h"

class AArenaUnit;
class UTexture2D;

// Définition d'une carte. Chaque carte du jeu est un Data Asset `DA_Carte_*` créé à partir de cette classe
// (clic droit dans le Content Browser > Miscellaneous > Data Asset > ArenaCardData).
// Les cartes ne changent jamais en jeu : elles ne sont que lues.
UCLASS(BlueprintType)
class ANERA_API UArenaCardData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Carte")
	FText DisplayName;

	// Référence « douce » : l'image n'est chargée en mémoire que lorsqu'on l'affiche.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Carte")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Carte", meta = (ClampMin = "1", ClampMax = "10"))
	int32 ElixirCost = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Carte")
	EArenaCardType CardType = EArenaCardType::Unit;

	// --- Unités ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unite", meta = (EditCondition = "CardType == EArenaCardType::Unit", EditConditionHides))
	TSubclassOf<AArenaUnit> UnitClass;

	// Nombre d'unités posées par la carte.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unite", meta = (ClampMin = "1", EditCondition = "CardType == EArenaCardType::Unit", EditConditionHides))
	int32 UnitCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unite", meta = (ClampMin = "1", EditCondition = "CardType == EArenaCardType::Unit", EditConditionHides))
	float MaxHealth = 100.f;

	// Vitesse de déplacement.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unite", meta = (ClampMin = "0", Units = "cm/s", EditCondition = "CardType == EArenaCardType::Unit", EditConditionHides))
	float MoveSpeed = 300.f;

	// Distance à laquelle l'unité repère une cible.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unite", meta = (ClampMin = "0", Units = "cm", EditCondition = "CardType == EArenaCardType::Unit", EditConditionHides))
	float SightRange = 600.f;

	// Unité volante : ignore la NavMesh et la rivière.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unite", meta = (EditCondition = "CardType == EArenaCardType::Unit", EditConditionHides))
	bool bFlying = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unite", meta = (EditCondition = "CardType == EArenaCardType::Unit", EditConditionHides))
	FArenaAttackStats Attack;

	// --- Sorts ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sort", meta = (ClampMin = "0", Units = "cm", EditCondition = "CardType == EArenaCardType::Spell", EditConditionHides))
	float SpellRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sort", meta = (ClampMin = "0", EditCondition = "CardType == EArenaCardType::Spell", EditConditionHides))
	float SpellDamage = 0.f;

	// Part des dégâts appliquée aux bâtiments (0,35 = 35 %).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sort", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "CardType == EArenaCardType::Spell", EditConditionHides))
	float BuildingDamageScale = 1.f;

	// Ralentissement des unités dans la zone (0,5 = 50 % plus lentes).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sort", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "CardType == EArenaCardType::Spell", EditConditionHides))
	float SlowAmount = 0.f;

	// Durée de l'effet au sol (0 = effet instantané).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sort", meta = (ClampMin = "0", Units = "s", EditCondition = "CardType == EArenaCardType::Spell", EditConditionHides))
	float EffectDuration = 0.f;
};

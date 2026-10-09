#pragma once

#include "CoreMinimal.h"
#include "ArenaTypes.generated.h"

class AArenaProjectile;

// Camp d'un joueur : J = joueur 1 (hôte), A = adversaire (joueur 2).
UENUM(BlueprintType)
enum class EArenaTeam : uint8
{
	None UMETA(DisplayName = "Aucun"),
	J UMETA(DisplayName = "Joueur (J)"),
	A UMETA(DisplayName = "Adversaire (A)"),
};

// Phase de la partie, répliquée par le GameState.
UENUM(BlueprintType)
enum class EArenaMatchPhase : uint8
{
	Waiting UMETA(DisplayName = "Attente"),
	InProgress UMETA(DisplayName = "En cours"),
	Overtime UMETA(DisplayName = "Prolongations"),
	Ended UMETA(DisplayName = "Fin"),
};

// Une carte fait apparaître des unités ou lance un sort.
UENUM(BlueprintType)
enum class EArenaCardType : uint8
{
	Unit UMETA(DisplayName = "Unité"),
	Spell UMETA(DisplayName = "Sort"),
};

// Ce qu'une attaque peut viser.
UENUM(BlueprintType)
enum class EArenaTargetType : uint8
{
	Ground UMETA(DisplayName = "Sol"),
	Air UMETA(DisplayName = "Air"),
	GroundAndAir UMETA(DisplayName = "Sol et air"),
	BuildingsOnly UMETA(DisplayName = "Bâtiments uniquement"),
};

UENUM(BlueprintType)
enum class EArenaBuildingType : uint8
{
	Tower UMETA(DisplayName = "Tour"),
	Fort UMETA(DisplayName = "Fort"),
};

// État visuel d'un bâtiment, piloté par ses PV (voir PLAN.md, 1.4).
UENUM(BlueprintType)
enum class EArenaBuildingState : uint8
{
	Intact UMETA(DisplayName = "Intact"),
	Damaged UMETA(DisplayName = "Endommagé"),
	Ruined UMETA(DisplayName = "Ruine"),
};

// Caractéristiques d'une attaque : partagées par les unités (via leur carte) et les bâtiments.
// Distances en uu (1 uu = 1 cm), durées en secondes.
USTRUCT(BlueprintType)
struct FArenaAttackStats
{
	GENERATED_BODY()

	// Dégâts par coup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attaque", meta = (ClampMin = "0"))
	float Damage = 0.f;

	// Temps entre deux coups.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attaque", meta = (ClampMin = "0.05", Units = "s"))
	float Interval = 1.f;

	// Portée de l'attaque.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attaque", meta = (ClampMin = "0", Units = "cm"))
	float Range = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attaque")
	EArenaTargetType TargetType = EArenaTargetType::Ground;

	// Rayon des dégâts de zone autour du point d'impact (0 = une seule cible).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attaque", meta = (ClampMin = "0", Units = "cm"))
	float SplashRadius = 0.f;

	// Projectile tiré à chaque coup (vide = attaque au contact, dégâts immédiats).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attaque")
	TSubclassOf<AArenaProjectile> ProjectileClass;
};

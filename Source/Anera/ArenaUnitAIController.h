#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ArenaUnitAIController.generated.h"

class AArenaUnit;

// Cerveau d'une unité, sans Behavior Tree : chercher une cible, aller vers elle, l'attaquer,
// et sinon suivre le chemin de sa voie (CHEMIN_IA_*). Un AIController n'existe que sur le serveur.
// Squelette : la logique arrive en 1.2.5.
UCLASS()
class ANERA_API AArenaUnitAIController : public AAIController
{
	GENERATED_BODY()

public:
	AArenaUnitAIController();

	UFUNCTION(BlueprintPure, Category = "Arene|IA")
	AArenaUnit* GetUnit() const;
};

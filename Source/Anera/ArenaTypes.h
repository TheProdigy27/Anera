#pragma once

#include "CoreMinimal.h"
#include "ArenaTypes.generated.h"

// Camp d'un joueur : J = joueur 1 (hôte), A = adversaire (joueur 2).
UENUM(BlueprintType)
enum class EArenaTeam : uint8
{
	None UMETA(DisplayName = "Aucun"),
	J UMETA(DisplayName = "Joueur (J)"),
	A UMETA(DisplayName = "Adversaire (A)"),
};

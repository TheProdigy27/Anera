#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaGameMode.generated.h"

// Règles de la partie : démarre et termine la partie, fait apparaître bâtiments et unités.
// Le GameMode n'existe que sur le serveur, les clients n'en ont jamais de copie.
// Squelette vide pour l'instant : rempli en phase 1 (tâche 1.2.1).
UCLASS()
class ANERA_API AArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()
};

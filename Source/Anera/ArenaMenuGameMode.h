#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaMenuGameMode.generated.h"

// Mode de jeu de la carte du menu : pas de partie ni de pion, seulement le menu à l'écran,
// affiché par AArenaMenuPlayerController.
UCLASS()
class ANERA_API AArenaMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaMenuGameMode();
};

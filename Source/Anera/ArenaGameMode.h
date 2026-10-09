#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaGameMode.generated.h"

// Règles de la partie : démarre et termine la partie, fait apparaître bâtiments et unités.
// Le GameMode n'existe que sur le serveur, les clients n'en ont jamais de copie.
// Pour l'instant : choix des classes du joueur et attribution des camps.
UCLASS()
class ANERA_API AArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaGameMode.generated.h"

class AArenaAIOpponent;
class AArenaBuilding;
class AArenaPlayerState;
class UArenaCardData;

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

	// Point d'entrée unique pour jouer une carte, appelé par ServerPlayCard (joueur humain)
	// ou directement par l'IA. Vérifie élixir, main et zone, puis fait apparaître unités ou sort (1.2.4).
	bool TryPlayCard(AArenaPlayerState* Player, UArenaCardData* Card, const FVector& Location);

protected:
	// Durée du temps réglementaire.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie", meta = (ClampMin = "1", Units = "s"))
	float MatchDuration = 180.f;

	// Durée des prolongations en cas d'égalité.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie", meta = (ClampMin = "0", Units = "s"))
	float OvertimeDuration = 120.f;

	// Délai entre la pose d'une carte et l'apparition des unités.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie", meta = (ClampMin = "0", Units = "s"))
	float DeployDelay = 1.f;

	// Classe des tours et forts posés sur les SLOT_* (un Blueprint enfant d'AArenaBuilding).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Classes")
	TSubclassOf<AArenaBuilding> BuildingClass;

	// Classe de l'adversaire IA en solo.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Classes")
	TSubclassOf<AArenaAIOpponent> AIOpponentClass;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaGameMode.generated.h"

class AArenaAIOpponent;
class AArenaBuilding;
class AArenaGameState;
class AArenaPlayerState;
class UArenaCardData;

// Règles de la partie : démarre et termine la partie, fait apparaître bâtiments et unités.
// Le GameMode n'existe que sur le serveur, les clients n'en ont jamais de copie.
UCLASS()
class ANERA_API AArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaGameMode();

	// Appelée quand le monde commence à jouer (avant les BeginPlay des acteurs du niveau).
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;

	// Point d'entrée unique pour jouer une carte, appelé par ServerPlayCard (joueur humain)
	// ou directement par l'IA. Vérifie élixir, main et zone, puis fait apparaître unités ou sort (1.2.4).
	bool TryPlayCard(AArenaPlayerState* Player, UArenaCardData* Card, const FVector& Location);

protected:
	// Lance la partie dès qu'il y a assez de joueurs : 1 en solo (sans réseau), 2 en réseau.
	void TryStartMatch();
	void StartMatch();

	AArenaGameState* GetArenaGameState() const;

	// Durée du temps réglementaire.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie", meta = (ClampMin = "1", Units = "s"))
	float MatchDuration = 180.f;

	// Durée des prolongations en cas d'égalité.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie", meta = (ClampMin = "0", Units = "s"))
	float OvertimeDuration = 120.f;

	// Délai entre la pose d'une carte et l'apparition des unités.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie", meta = (ClampMin = "0", Units = "s"))
	float DeployDelay = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Elixir", meta = (ClampMin = "0"))
	float StartElixir = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Elixir", meta = (ClampMin = "1"))
	float MaxElixir = 10.f;

	// Temps pour gagner 1 élixir, au rythme normal.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Elixir", meta = (ClampMin = "0.1", Units = "s"))
	float ElixirRegenInterval = 2.8f;

	// Le rythme est multiplié quand il reste moins que ce temps de jeu réglementaire, et pendant les prolongations.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Elixir", meta = (ClampMin = "0", Units = "s"))
	float DoubleElixirTime = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Elixir", meta = (ClampMin = "1"))
	float DoubleElixirMultiplier = 2.f;

	// Classe des tours et forts posés sur les SLOT_* (un Blueprint enfant d'AArenaBuilding).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Classes")
	TSubclassOf<AArenaBuilding> BuildingClass;

	// Classe de l'adversaire IA en solo.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Classes")
	TSubclassOf<AArenaAIOpponent> AIOpponentClass;
};

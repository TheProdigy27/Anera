#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaTypes.h"
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
	// ou directement par l'IA. Vérifie partie, main, élixir et zone, puis fait apparaître unités ou sort.
	bool TryPlayCard(AArenaPlayerState* Player, UArenaCardData* Card, const FVector& Location);

protected:
	// Fait apparaître un AArenaBuilding sur chaque SLOT_Tour_* et SLOT_Fort_* du niveau.
	void SpawnBuildings();

	// Couronnes et réveil du fort quand un bâtiment tombe.
	UFUNCTION()
	void HandleBuildingDestroyed(AArenaBuilding* Building);

	// Fait apparaître les unités de la carte autour de Location (au sol), en formation si elles sont plusieurs.
	void SpawnUnits(UArenaCardData* Card, EArenaTeam Team, const FVector& Location);

	// Applique l'effet d'un sort (dégâts, ralentissement) une fois le délai écoulé.
	void ResolveSpell(UArenaCardData* Card, EArenaTeam Team, FVector Location);

	// Lance la partie dès qu'il y a assez de joueurs : 1 en solo (sans réseau), 2 en réseau.
	void TryStartMatch();
	void StartMatch();

	// Solo (partie sans réseau) : une IA joue le camp A. En mode démo, une autre IA joue aussi le camp J
	// avec le PlayerState du joueur.
	void SpawnAIOpponents();
	AArenaAIOpponent* SpawnAIOpponent(EArenaTeam Team, AArenaPlayerState* ExistingState);

	// Fin du temps réglementaire : prolongations si les couronnes sont à égalité, sinon fin de partie.
	void HandleRegularTimeOver();

	// Fin de partie. Winner = Aucun pour un match nul.
	void EndMatch(EArenaTeam Winner);

	// Départage à la fin des prolongations : le camp dont le bâtiment le plus abîmé a gardé
	// la plus grande part de ses PV gagne (bâtiments détruits exclus). Aucun en cas d'égalité.
	EArenaTeam GetTieBreakWinner() const;

	AArenaGameState* GetArenaGameState() const;

	// Deck d'un joueur au lancement de la partie.
	virtual TArray<UArenaCardData*> GetDeckFor(const AArenaPlayerState* Player) const;

	// Deck de chaque joueur tant que l'écran de choix du deck (phase 3) n'existe pas. 8 cartes.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Cartes")
	TArray<TObjectPtr<UArenaCardData>> DefaultDeck;

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

	// Test : en solo, une IA joue aussi le camp J à la place du joueur (partie IA contre IA à regarder).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie")
	bool bDemoMode = false;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Partie")
	TArray<TObjectPtr<AArenaBuilding>> Buildings;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Partie")
	TArray<TObjectPtr<AArenaAIOpponent>> AIOpponents;
};

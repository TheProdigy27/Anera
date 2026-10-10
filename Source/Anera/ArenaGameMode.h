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

// Élixir et cartes d'un joueur parti en cours de partie, rendus à celui qui reprend son camp.
USTRUCT()
struct FArenaAbsentPlayer
{
	GENERATED_BODY()

	// Continue de se remplir pendant l'absence, comme celui de l'adversaire.
	UPROPERTY()
	float Elixir = 0.f;

	// La main puis la pile, dans l'ordre.
	UPROPERTY()
	TArray<TObjectPtr<UArenaCardData>> Cards;
};

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

	// L'arène se joue à 2 : un troisième joueur est refusé, comme un joueur qui arrive après la fin
	// (un ErrorMessage non vide refuse la connexion, et le message est affiché dans le menu du joueur refusé).
	// PreLogin, appelée avant le chargement de la carte chez le client, le refuse tôt. Login refait le contrôle :
	// deux clients qui se connectent en même temps passent tous deux PreLogin avant d'avoir un PlayerState.
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// Un joueur qui quitte une partie en cours laisse sa place libre : élixir et cartes sont gardés pour son retour.
	virtual void Logout(AController* Exiting) override;
	virtual void Tick(float DeltaSeconds) override;

	// Point d'entrée unique pour jouer une carte, appelé par ServerPlayCard (joueur humain)
	// ou directement par l'IA. Vérifie partie, main, élixir et zone, puis fait apparaître unités ou sort.
	bool TryPlayCard(AArenaPlayerState* Player, UArenaCardData* Card, const FVector& Location);

protected:
	// Vrai quand 2 joueurs humains sont déjà là (les IA du solo, des robots, ne comptent pas).
	bool IsFull() const;

	// Raison de refuser un nouveau joueur (partie complète ou terminée), ou nullptr s'il peut entrer.
	const TCHAR* GetJoinRefusal() const;

	// Élixir et cartes d'un joueur au lancement de la partie, ou à son arrivée en cours de partie
	// (il reprend alors ce qu'avait laissé son camp, s'il y était déjà).
	void InitializePlayerForMatch(AArenaPlayerState* Player);

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

	// Solo (partie sans réseau) : une IA joue le camp A. En mode démo, une IA joue aussi à la place
	// de chaque joueur humain, avec son PlayerState (en solo comme en réseau).
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

	// Test : une IA joue à la place de chaque joueur humain (partie IA contre IA à regarder, en solo ou en réseau).
	// Aussi activé par la commande console « Arene.Demo 1 » avant le début de la partie, sans toucher au Blueprint.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Partie")
	bool bDemoMode = false;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Partie")
	TArray<TObjectPtr<AArenaBuilding>> Buildings;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Partie")
	TArray<TObjectPtr<AArenaAIOpponent>> AIOpponents;

	// Camps dont le joueur est parti en cours de partie.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|Partie")
	TMap<EArenaTeam, FArenaAbsentPlayer> AbsentPlayers;
};

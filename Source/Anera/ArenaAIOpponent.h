#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "ArenaTypes.h"
#include "ArenaAIOpponent.generated.h"

class AArenaPlayerState;
class UArenaCardData;

// Adversaire IA du mode solo. Un AInfo est un acteur sans représentation dans le monde, fait pour
// porter de la logique. Il n'existe que sur le serveur et joue avec son propre AArenaPlayerState
// (élixir, deck, main), en passant par le même AArenaGameMode::TryPlayCard qu'un joueur humain.
//
// Stratégie basique : une carte de la main au hasard, posée dès que l'élixir le permet.
// Unités : devant un ennemi entré dans son camp, sinon au départ d'une voie au hasard.
// Sorts : sur l'unité ennemie la plus avancée (jamais lancés dans le vide).
// Blueprintable : AInfo l'interdit par défaut, on le réautorise pour régler l'IA dans un Blueprint enfant.
UCLASS(Blueprintable)
class ANERA_API AArenaAIOpponent : public AInfo
{
	GENERATED_BODY()

public:
	AArenaAIOpponent();

	// Serveur, avant BeginPlay : jouer avec ce PlayerState existant (celui du joueur, en mode démo)
	// au lieu d'en créer un.
	void SetControlledState(AArenaPlayerState* State);

	UFUNCTION(BlueprintPure, Category = "Arene|IA")
	EArenaTeam GetTeam() const { return Team; }

	void SetTeam(EArenaTeam NewTeam) { Team = NewTeam; }

	AArenaPlayerState* GetOpponentState() const { return OpponentState; }

	// Deck propre à l'IA ; vide = le deck par défaut du GameMode.
	TArray<UArenaCardData*> GetDeck() const { return ObjectPtrDecay(Deck); }

protected:
	virtual void BeginPlay() override;

	void Think();

	// Une carte de la main au hasard, parmi celles utiles maintenant.
	UArenaCardData* ChooseCard() const;

	// Point où poser la carte. Faux si aucun point valable n'a été trouvé.
	bool ChooseLocation(const UArenaCardData* Card, FVector& OutLocation) const;

	// Unité ennemie la plus proche de notre fort (la plus menaçante), ou nullptr.
	AActor* FindMostAdvancedEnemy(bool bOnlyInOwnZone) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|IA")
	EArenaTeam Team = EArenaTeam::A;

	// Temps entre deux décisions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|IA", meta = (ClampMin = "0.1", Units = "s"))
	float DecisionInterval = 0.5f;

	// Pause tirée au hasard dans cet intervalle après chaque carte posée.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|IA")
	FVector2D PauseAfterPlay = FVector2D(1.f, 4.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|IA")
	TArray<TObjectPtr<UArenaCardData>> Deck;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|IA")
	TObjectPtr<AArenaPlayerState> OpponentState;

	// Carte choisie, en attente d'assez d'élixir.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|IA")
	TObjectPtr<UArenaCardData> PendingCard;

private:
	FTimerHandle ThinkTimer;
	double NextDecisionTime = 0.0;
	int32 FailedAttempts = 0;
};

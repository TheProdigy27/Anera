#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "ArenaTypes.h"
#include "ArenaAIOpponent.generated.h"

class AArenaPlayerState;

// Adversaire IA du mode solo. Un AInfo est un acteur sans représentation dans le monde, fait pour
// porter de la logique. Il n'existe que sur le serveur et joue avec son propre AArenaPlayerState
// (élixir, deck, main), en passant par le même AArenaGameMode::TryPlayCard qu'un joueur humain.
// Squelette : la logique arrive en 1.2.8.
// Blueprintable : AInfo l'interdit par défaut, on le réautorise pour régler l'IA dans un Blueprint enfant.
UCLASS(Blueprintable)
class ANERA_API AArenaAIOpponent : public AInfo
{
	GENERATED_BODY()

public:
	AArenaAIOpponent();

	UFUNCTION(BlueprintPure, Category = "Arene|IA")
	EArenaTeam GetTeam() const { return Team; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|IA")
	EArenaTeam Team = EArenaTeam::A;

	// Temps entre deux décisions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arene|IA", meta = (ClampMin = "0.1", Units = "s"))
	float DecisionInterval = 1.f;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arene|IA")
	TObjectPtr<AArenaPlayerState> OpponentState;
};

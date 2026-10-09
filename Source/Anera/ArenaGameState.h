#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArenaTypes.h"
#include "ArenaGameState.generated.h"

// État de la partie, visible par tous les clients : phase, chrono, couronnes et vainqueur.
// Contrairement au GameMode (serveur uniquement), le GameState existe chez chaque joueur et
// ses variables Replicated y sont recopiées. Seul le serveur les modifie (via le GameMode).
UCLASS()
class ANERA_API AArenaGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Arene|Partie")
	EArenaMatchPhase GetMatchPhase() const { return MatchPhase; }

	// Vrai pendant le temps réglementaire et les prolongations : on peut jouer des cartes.
	UFUNCTION(BlueprintPure, Category = "Arene|Partie")
	bool IsMatchRunning() const { return MatchPhase == EArenaMatchPhase::InProgress || MatchPhase == EArenaMatchPhase::Overtime; }

	// Vrai quand l'élixir se recharge deux fois plus vite (dernière minute et prolongations).
	UFUNCTION(BlueprintPure, Category = "Arene|Partie")
	bool IsDoubleElixir() const { return bDoubleElixir; }

	// Secondes avant la fin de la phase en cours (0 hors chrono).
	UFUNCTION(BlueprintPure, Category = "Arene|Partie")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Arene|Partie")
	int32 GetCrowns(EArenaTeam Team) const;

	// Vainqueur une fois la partie finie (Aucun = match nul).
	UFUNCTION(BlueprintPure, Category = "Arene|Partie")
	EArenaTeam GetWinner() const { return Winner; }

	// Serveur uniquement. Duration <= 0 : pas de chrono pour cette phase.
	void SetMatchPhase(EArenaMatchPhase NewPhase, float Duration);
	void AddCrowns(EArenaTeam Team, int32 Count);
	void SetCrowns(EArenaTeam Team, int32 Count);
	void SetWinner(EArenaTeam NewWinner);
	void SetDoubleElixir(bool bEnabled);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Partie")
	EArenaMatchPhase MatchPhase = EArenaMatchPhase::Waiting;

	// Heure de fin de la phase, sur l'horloge du serveur. On réplique cette heure une seule fois
	// plutôt que le temps restant à chaque image : chaque client en déduit le chrono lui-même.
	UPROPERTY(Replicated)
	double PhaseEndTime = 0.0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Partie")
	int32 CrownsJ = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Partie")
	int32 CrownsA = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Partie")
	EArenaTeam Winner = EArenaTeam::None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene|Partie")
	bool bDoubleElixir = false;
};

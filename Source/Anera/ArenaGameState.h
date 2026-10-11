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

	// Serveur uniquement, à la revanche : couronnes à zéro, pas de vainqueur, et retour à l'attente des joueurs.
	void ResetMatch();

	// Serveur : fait jouer un effet visuel (impact, zone de sort) chez tous les joueurs, hôte compris.
	// Les effets d'une même image partent ensemble à l'image suivante : un RPC non fiable pour les coups et
	// les impacts, un RPC fiable pour les sorts.
	void PlayEffect(const FArenaEffect& Effect);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Joue un effet sur cette machine. Par défaut, un marqueur provisoire (AArenaEffectMarker) ;
	// en phase 4, BP_ArenaGameState pourra le remplacer par des effets Niagara et des sons.
	// BlueprintNativeEvent : fonction C++ qu'un Blueprint enfant peut redéfinir.
	UFUNCTION(BlueprintNativeEvent, Category = "Arene|Effets")
	void SpawnEffect(const FArenaEffect& Effect);

	// Marqueur provisoire créé par SpawnEffect (vide = AArenaEffectMarker).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Effets")
	TSubclassOf<class AArenaEffectMarker> EffectMarkerClass;

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

private:
	// RPC multicast : appelé sur le serveur, exécuté sur le serveur et chez chaque client.
	// Unreliable (non fiable) : un paquet perdu n'est pas renvoyé. Cela convient aux coups et aux impacts,
	// fréquents et brefs, qu'il ne servirait à rien d'afficher en retard.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayEffects(const TArray<FArenaEffect>& Effects);

	// Les sorts passent par un RPC fiable (Reliable), renvoyé jusqu'à ce qu'il arrive. Leur zone reste au sol
	// et accompagne un effet de jeu (ralentissement, dégâts sur plusieurs unités) : son absence se verrait.
	// Ils sont rares, et les renvois ne coûtent presque rien.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySpellEffects(const TArray<FArenaEffect>& Effects);

	void SendPendingEffects();
	void SpawnEffects(const TArray<FArenaEffect>& Effects);

	TArray<FArenaEffect> PendingEffects;
	bool bEffectSendScheduled = false;
};

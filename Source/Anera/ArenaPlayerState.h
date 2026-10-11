#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArenaTypes.h"
#include "ArenaPlayerState.generated.h"

class UArenaCardData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaHandChangedSignature);

// État d'un joueur, répliqué : le camp est visible de tous, l'élixir et les cartes du seul joueur concerné.
// Seul le serveur modifie ces valeurs (le GameMode pour l'élixir, la pose des cartes pour la main).
UCLASS()
class ANERA_API AArenaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	static constexpr int32 HandSize = 4;

	AArenaPlayerState();

	EArenaTeam GetTeam() const { return Team; }

	// Serveur uniquement : le GameMode attribue le camp à la connexion.
	void SetTeam(EArenaTeam NewTeam);

	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	float GetElixir() const { return Elixir; }

	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	float GetMaxElixir() const { return MaxElixir; }

	// Serveur uniquement : élixir de départ et plafond, au lancement de la partie.
	void InitializeElixir(float StartElixir, float InMaxElixir);

	// Serveur uniquement : ajoute de l'élixir, sans dépasser le plafond.
	void AddElixir(float Amount);

	// Serveur uniquement : retire Cost s'il y a assez d'élixir. Renvoie false sinon, sans rien retirer.
	bool SpendElixir(float Cost);

	// Serveur uniquement : mélange le deck (sauf bShuffle = false), en tire la main (4 cartes) et la carte suivante.
	void InitializeDeck(const TArray<UArenaCardData*>& Cards, bool bShuffle = true);

	// Serveur uniquement : la main puis la pile, dans l'ordre. Rendues telles quelles par InitializeDeck(…, false)
	// au joueur qui revient après une déconnexion.
	TArray<UArenaCardData*> GetDeckInOrder() const;

	// Position de la carte dans la main, ou INDEX_NONE si elle n'y est pas.
	int32 FindCardInHand(const UArenaCardData* Card) const;

	// Serveur uniquement : la carte suivante prend la place de la carte jouée, qui retourne sous la pile.
	void CycleCard(int32 HandIndex);

	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	TArray<UArenaCardData*> GetHand() const { return ObjectPtrDecay(Hand); }

	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	UArenaCardData* GetNextCard() const { return NextCard; }

	// Numéro de la dernière demande de pose de carte traitée par le serveur, acceptée ou refusée.
	int32 GetLastCardRequestId() const { return LastCardRequestId; }

	// Serveur uniquement : la demande RequestId du joueur est traitée. Main, élixir et numéro partent tout de suite.
	void ConfirmCardRequest(int32 RequestId);

	// Serveur uniquement : deck choisi par le joueur dans le menu, reçu à son arrivée et déjà vérifié
	// (vide : deck par défaut). Il est distribué à chaque début de partie, revanches comprises.
	void SetChosenDeck(const TArray<UArenaCardData*>& Cards);
	TArray<UArenaCardData*> GetChosenDeck() const { return ObjectPtrDecay(ChosenDeck); }

	// Vrai si le joueur a demandé une revanche sur l'écran de fin. Visible de tous : l'adversaire le voit.
	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	bool WantsRematch() const { return bWantsRematch; }

	// Serveur uniquement.
	void SetWantsRematch(bool bInWantsRematch);

	// Serveur uniquement, à la revanche : plus de cartes ni d'élixir jusqu'à la distribution du début de partie.
	void ResetForNewMatch();

	// Serveur et propriétaire : la main ou la carte suivante ont changé (pour l'interface).
	UPROPERTY(BlueprintAssignable, Category = "Arene|Joueur")
	FArenaHandChangedSignature OnHandChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Répliqué : le serveur fixe la valeur, Unreal la recopie chez les clients.
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene")
	EArenaTeam Team = EArenaTeam::None;

	// Élixir disponible (fractionnaire : il se remplit en continu). Répliqué au seul propriétaire.
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene")
	float Elixir = 0.f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene")
	float MaxElixir = 10.f;

	// Les cartes du deck qui ne sont pas en main, dans l'ordre : la première est la carte suivante.
	// Connue du serveur seulement.
	UPROPERTY(VisibleInstanceOnly, Category = "Arene")
	TArray<TObjectPtr<UArenaCardData>> DrawPile;

	// Les 4 cartes jouables. Répliquées au seul propriétaire : l'adversaire ne voit pas votre main.
	UPROPERTY(ReplicatedUsing = OnRep_Cards, VisibleInstanceOnly, Category = "Arene")
	TArray<TObjectPtr<UArenaCardData>> Hand;

	UPROPERTY(ReplicatedUsing = OnRep_Cards, VisibleInstanceOnly, Category = "Arene")
	TObjectPtr<UArenaCardData> NextCard;

	// Répliqué au seul propriétaire, dans la même mise à jour que la main et l'élixir : quand le client voit
	// ce numéro passer, sa main et son élixir tiennent déjà compte de la carte (voir AArenaPlayerController).
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene")
	int32 LastCardRequestId = 0;

	// Connu du serveur seulement.
	UPROPERTY(VisibleInstanceOnly, Category = "Arene")
	TArray<TObjectPtr<UArenaCardData>> ChosenDeck;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene")
	bool bWantsRematch = false;

	UFUNCTION()
	void OnRep_Cards();
};

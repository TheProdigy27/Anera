#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArenaTypes.h"
#include "ArenaPlayerState.generated.h"

class UArenaCardData;

// État d'un joueur, répliqué : le camp est visible de tous, l'élixir et les cartes du seul joueur concerné.
// Squelette : régénération de l'élixir en 1.2.2, pioche en 1.2.3.
UCLASS()
class ANERA_API AArenaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	EArenaTeam GetTeam() const { return Team; }

	// Serveur uniquement : le GameMode attribue le camp à la connexion.
	void SetTeam(EArenaTeam NewTeam);

	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	float GetElixir() const { return Elixir; }

	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	TArray<UArenaCardData*> GetHand() const { return ObjectPtrDecay(Hand); }

	UFUNCTION(BlueprintPure, Category = "Arene|Joueur")
	UArenaCardData* GetNextCard() const { return NextCard; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Répliqué : le serveur fixe la valeur, Unreal la recopie chez les clients.
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene")
	EArenaTeam Team = EArenaTeam::None;

	// Élixir disponible (fractionnaire : il se remplit en continu). Répliqué au seul propriétaire.
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene")
	float Elixir = 0.f;

	// Les 8 cartes du deck, dans l'ordre de la pioche. Connu du serveur seulement.
	UPROPERTY(VisibleInstanceOnly, Category = "Arene")
	TArray<TObjectPtr<UArenaCardData>> Deck;

	// Les 4 cartes jouables. Répliquées au seul propriétaire : l'adversaire ne voit pas votre main.
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene")
	TArray<TObjectPtr<UArenaCardData>> Hand;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Arene")
	TObjectPtr<UArenaCardData> NextCard;
};

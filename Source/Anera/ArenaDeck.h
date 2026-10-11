#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ArenaDeck.generated.h"

class UArenaCardData;

// Deck enregistré sur le disque par l'écran « Deck » du menu (Saved/SaveGames/Deck.sav).
// Un USaveGame est un objet dont les UPROPERTY sont écrites dans un fichier par UGameplayStatics::SaveGameToSlot,
// puis relues par LoadGameFromSlot.
UCLASS()
class ANERA_API UArenaDeckSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Cartes du deck, par identifiant de l'Asset Manager : type « ArenaCardData », nom de l'asset (« DA_Carte_Ecuyer »).
	UPROPERTY()
	TArray<FPrimaryAssetId> Cards;
};

// Cartes disponibles et decks des joueurs.
// Les cartes du jeu sont les Data Assets ArenaCardData que l'Asset Manager répertorie dans /Game/Arene/Cartes
// (PrimaryAssetTypesToScan, dans DefaultGame.ini) : une nouvelle carte y apparaît sans rien changer d'autre,
// et elle est toujours incluse dans le jeu packagé.
namespace ArenaDeck
{
	// Nombre de cartes d'un deck.
	constexpr int32 DeckSize = 8;

	// Toutes les cartes du jeu, triées par coût puis par nom (chargées si besoin).
	TArray<UArenaCardData*> GetAvailableCards();

	// Vrai si ces cartes forment un deck jouable : DeckSize cartes différentes, toutes parmi les cartes du jeu.
	bool IsValidDeck(const TArray<UArenaCardData*>& Cards);

	// Coût moyen en élixir (0 pour un deck vide).
	float GetAverageCost(const TArray<UArenaCardData*>& Cards);

	// Option d'URL qui transmet le deck au serveur à l'entrée dans l'arène : « Deck=DA_Carte_Ecuyer,DA_Carte_Milice,… ».
	// Vide si le deck est vide.
	FString MakeDeckOption(const TArray<UArenaCardData*>& Cards);

	// Serveur : cartes de l'option « Deck » des options de connexion d'un joueur. Vide si l'option manque, si elle n'a
	// pas DeckSize cartes ou si une carte est inconnue. Le texte vient du client : le deck est encore à vérifier
	// avec IsValidDeck (cartes en double).
	TArray<UArenaCardData*> ParseDeckOption(const FString& Options);

	// Deck enregistré. Vide s'il n'y en a pas, ou s'il n'est plus jouable (carte retirée du jeu…).
	TArray<UArenaCardData*> LoadSavedDeck();

	// Enregistre le deck. Faux s'il n'est pas jouable ou si l'écriture échoue.
	bool SaveDeck(const TArray<UArenaCardData*>& Cards);
}

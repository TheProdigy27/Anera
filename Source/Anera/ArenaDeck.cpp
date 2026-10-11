#include "ArenaDeck.h"
#include "Anera.h"
#include "ArenaCardData.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Fichier du deck : Saved/SaveGames/Deck.sav.
	const TCHAR* DeckSlotName = TEXT("Deck");
	constexpr int32 DeckUserIndex = 0;

	FPrimaryAssetType GetCardAssetType()
	{
		// Type donné par l'Asset Manager aux Data Assets d'UArenaCardData : le nom de leur classe C++.
		return FPrimaryAssetType(TEXT("ArenaCardData"));
	}

	// Carte de cet identifiant, chargée si besoin. Null si l'Asset Manager ne la connaît pas.
	UArenaCardData* LoadCard(const FPrimaryAssetId& Id)
	{
		const UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		if (!AssetManager || !Id.IsValid())
		{
			return nullptr;
		}
		const FSoftObjectPath Path = AssetManager->GetPrimaryAssetPath(Id);
		return Path.IsValid() ? Cast<UArenaCardData>(Path.TryLoad()) : nullptr;
	}

	bool IsAvailableCard(const UArenaCardData* Card)
	{
		const UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		return Card && AssetManager && AssetManager->GetPrimaryAssetPath(Card->GetPrimaryAssetId()).IsValid();
	}
}

TArray<UArenaCardData*> ArenaDeck::GetAvailableCards()
{
	TArray<UArenaCardData*> Cards;
	const UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	TArray<FPrimaryAssetId> Ids;
	if (!AssetManager || !AssetManager->GetPrimaryAssetIdList(GetCardAssetType(), Ids))
	{
		UE_LOG(LogArene, Warning, TEXT("Aucune carte trouvee : PrimaryAssetTypesToScan doit contenir le type ArenaCardData (DefaultGame.ini)."));
		return Cards;
	}
	for (const FPrimaryAssetId& Id : Ids)
	{
		if (UArenaCardData* Card = LoadCard(Id))
		{
			Cards.Add(Card);
		}
	}
	Cards.Sort([](const UArenaCardData& Left, const UArenaCardData& Right)
	{
		return Left.ElixirCost != Right.ElixirCost ? Left.ElixirCost < Right.ElixirCost : Left.DisplayName.CompareTo(Right.DisplayName) < 0;
	});
	return Cards;
}

bool ArenaDeck::IsValidDeck(const TArray<UArenaCardData*>& Cards)
{
	if (Cards.Num() != DeckSize)
	{
		return false;
	}
	TSet<const UArenaCardData*> Seen;
	for (const UArenaCardData* Card : Cards)
	{
		bool bAlreadySeen = false;
		Seen.Add(Card, &bAlreadySeen);
		if (bAlreadySeen || !IsAvailableCard(Card))
		{
			return false;
		}
	}
	return true;
}

float ArenaDeck::GetAverageCost(const TArray<UArenaCardData*>& Cards)
{
	int32 Total = 0;
	int32 Count = 0;
	for (const UArenaCardData* Card : Cards)
	{
		if (Card)
		{
			Total += Card->ElixirCost;
			++Count;
		}
	}
	return Count > 0 ? static_cast<float>(Total) / Count : 0.f;
}

FString ArenaDeck::MakeDeckOption(const TArray<UArenaCardData*>& Cards)
{
	TArray<FString> Names;
	for (const UArenaCardData* Card : Cards)
	{
		if (Card)
		{
			Names.Add(Card->GetPrimaryAssetId().PrimaryAssetName.ToString());
		}
	}
	return Names.IsEmpty() ? FString() : TEXT("Deck=") + FString::Join(Names, TEXT(","));
}

TArray<UArenaCardData*> ArenaDeck::ParseDeckOption(const FString& Options)
{
	TArray<FString> Names;
	UGameplayStatics::ParseOption(Options, TEXT("Deck")).ParseIntoArray(Names, TEXT(","));
	if (Names.Num() != DeckSize)
	{
		return {};
	}
	TArray<UArenaCardData*> Cards;
	for (const FString& Name : Names)
	{
		// FNAME_Find : cherche un nom déjà connu sans en créer de nouveau, puisque le texte vient du client.
		// Une carte du jeu a forcément déjà son nom dans la table des FName.
		const FName AssetName(*Name, FNAME_Find);
		UArenaCardData* Card = AssetName.IsNone() ? nullptr : LoadCard(FPrimaryAssetId(GetCardAssetType(), AssetName));
		if (!Card)
		{
			return {};
		}
		Cards.Add(Card);
	}
	return Cards;
}

TArray<UArenaCardData*> ArenaDeck::LoadSavedDeck()
{
	if (!UGameplayStatics::DoesSaveGameExist(DeckSlotName, DeckUserIndex))
	{
		return {};
	}
	const UArenaDeckSaveGame* Save = Cast<UArenaDeckSaveGame>(UGameplayStatics::LoadGameFromSlot(DeckSlotName, DeckUserIndex));
	if (!Save)
	{
		UE_LOG(LogArene, Warning, TEXT("Le deck enregistre n'a pas pu etre lu : deck par defaut."));
		return {};
	}
	TArray<UArenaCardData*> Cards;
	for (const FPrimaryAssetId& Id : Save->Cards)
	{
		Cards.Add(LoadCard(Id));
	}
	if (!IsValidDeck(Cards))
	{
		UE_LOG(LogArene, Warning, TEXT("Le deck enregistre n'est plus jouable (carte retiree du jeu ?) : deck par defaut."));
		return {};
	}
	return Cards;
}

bool ArenaDeck::SaveDeck(const TArray<UArenaCardData*>& Cards)
{
	if (!IsValidDeck(Cards))
	{
		return false;
	}
	UArenaDeckSaveGame* Save = Cast<UArenaDeckSaveGame>(UGameplayStatics::CreateSaveGameObject(UArenaDeckSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	for (const UArenaCardData* Card : Cards)
	{
		Save->Cards.Add(Card->GetPrimaryAssetId());
	}
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, DeckSlotName, DeckUserIndex);
	UE_CLOG(!bSaved, LogArene, Warning, TEXT("Le deck n'a pas pu etre enregistre."));
	return bSaved;
}

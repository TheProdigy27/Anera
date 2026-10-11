#include "ArenaPlayerState.h"
#include "ArenaCardData.h"
#include "Net/UnrealNetwork.h"

AArenaPlayerState::AArenaPlayerState()
{
	// Par défaut, un PlayerState n'envoie ses changements qu'une fois par seconde :
	// trop peu pour une jauge d'élixir qui se remplit en continu.
	SetNetUpdateFrequency(10.f);
}

void AArenaPlayerState::SetTeam(EArenaTeam NewTeam)
{
	check(HasAuthority());
	Team = NewTeam;
}

void AArenaPlayerState::InitializeElixir(float StartElixir, float InMaxElixir)
{
	check(HasAuthority());
	MaxElixir = FMath::Max(0.f, InMaxElixir);
	Elixir = FMath::Clamp(StartElixir, 0.f, MaxElixir);
}

void AArenaPlayerState::AddElixir(float Amount)
{
	check(HasAuthority());
	Elixir = FMath::Clamp(Elixir + Amount, 0.f, MaxElixir);
}

bool AArenaPlayerState::SpendElixir(float Cost)
{
	check(HasAuthority());
	if (Cost > Elixir)
	{
		return false;
	}
	Elixir -= Cost;
	return true;
}

void AArenaPlayerState::InitializeDeck(const TArray<UArenaCardData*>& Cards, bool bShuffle)
{
	check(HasAuthority());

	TArray<UArenaCardData*> Shuffled;
	for (UArenaCardData* Card : Cards)
	{
		if (Card)
		{
			Shuffled.Add(Card);
		}
	}
	// Mélange de Fisher-Yates.
	for (int32 Index = Shuffled.Num() - 1; bShuffle && Index > 0; --Index)
	{
		Shuffled.Swap(Index, FMath::RandRange(0, Index));
	}

	const int32 HandCount = FMath::Min(HandSize, Shuffled.Num());
	Hand.Reset();
	DrawPile.Reset();
	for (int32 Index = 0; Index < Shuffled.Num(); ++Index)
	{
		(Index < HandCount ? Hand : DrawPile).Add(Shuffled[Index]);
	}
	NextCard = DrawPile.IsEmpty() ? nullptr : DrawPile[0];
	OnHandChanged.Broadcast();
}

TArray<UArenaCardData*> AArenaPlayerState::GetDeckInOrder() const
{
	TArray<UArenaCardData*> Cards = ObjectPtrDecay(Hand);
	Cards.Append(ObjectPtrDecay(DrawPile));
	return Cards;
}

int32 AArenaPlayerState::FindCardInHand(const UArenaCardData* Card) const
{
	for (int32 Index = 0; Index < Hand.Num(); ++Index)
	{
		if (Card && Hand[Index] == Card)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AArenaPlayerState::CycleCard(int32 HandIndex)
{
	check(HasAuthority());
	if (!Hand.IsValidIndex(HandIndex) || DrawPile.IsEmpty())
	{
		// Deck de 4 cartes ou moins : la carte reste en main.
		return;
	}

	UArenaCardData* Played = Hand[HandIndex];
	Hand[HandIndex] = DrawPile[0];
	DrawPile.RemoveAt(0);
	DrawPile.Add(Played);
	NextCard = DrawPile[0];
	OnHandChanged.Broadcast();
}

void AArenaPlayerState::ConfirmCardRequest(int32 RequestId)
{
	check(HasAuthority());
	LastCardRequestId = FMath::Max(LastCardRequestId, RequestId);
	// Sans attendre la prochaine mise à jour du PlayerState (10 par seconde) : le joueur attend la réponse.
	ForceNetUpdate();
}

void AArenaPlayerState::SetChosenDeck(const TArray<UArenaCardData*>& Cards)
{
	check(HasAuthority());
	ChosenDeck = Cards;
}

void AArenaPlayerState::SetWantsRematch(bool bInWantsRematch)
{
	check(HasAuthority());
	bWantsRematch = bInWantsRematch;
	// L'adversaire attend peut-être cette réponse sur son écran de fin.
	ForceNetUpdate();
}

void AArenaPlayerState::ResetForNewMatch()
{
	check(HasAuthority());
	Elixir = 0.f;
	Hand.Reset();
	DrawPile.Reset();
	NextCard = nullptr;
	bWantsRematch = false;
	OnHandChanged.Broadcast();
}

void AArenaPlayerState::OnRep_Cards()
{
	OnHandChanged.Broadcast();
}

void AArenaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaPlayerState, Team);
	DOREPLIFETIME(AArenaPlayerState, bWantsRematch);
	// COND_OwnerOnly : envoyé uniquement au joueur à qui appartient ce PlayerState.
	DOREPLIFETIME_CONDITION(AArenaPlayerState, Elixir, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, MaxElixir, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, Hand, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, NextCard, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, LastCardRequestId, COND_OwnerOnly);
}

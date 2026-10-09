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

void AArenaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaPlayerState, Team);
	// COND_OwnerOnly : envoyé uniquement au joueur à qui appartient ce PlayerState.
	DOREPLIFETIME_CONDITION(AArenaPlayerState, Elixir, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, MaxElixir, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, Hand, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, NextCard, COND_OwnerOnly);
}

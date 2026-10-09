#include "ArenaPlayerState.h"
#include "ArenaCardData.h"
#include "Net/UnrealNetwork.h"

void AArenaPlayerState::SetTeam(EArenaTeam NewTeam)
{
	check(HasAuthority());
	Team = NewTeam;
}

void AArenaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaPlayerState, Team);
	// COND_OwnerOnly : envoyé uniquement au joueur à qui appartient ce PlayerState.
	DOREPLIFETIME_CONDITION(AArenaPlayerState, Elixir, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, Hand, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, NextCard, COND_OwnerOnly);
}

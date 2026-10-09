#include "ArenaPlayerState.h"
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
}

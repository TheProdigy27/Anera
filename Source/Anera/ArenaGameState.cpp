#include "ArenaGameState.h"
#include "Net/UnrealNetwork.h"

float AArenaGameState::GetRemainingTime() const
{
	if (PhaseEndTime <= 0.0)
	{
		return 0.f;
	}
	// GetServerWorldTimeSeconds : l'heure du serveur, estimée chez les clients.
	return static_cast<float>(FMath::Max(0.0, PhaseEndTime - GetServerWorldTimeSeconds()));
}

int32 AArenaGameState::GetCrowns(EArenaTeam Team) const
{
	switch (Team)
	{
	case EArenaTeam::J: return CrownsJ;
	case EArenaTeam::A: return CrownsA;
	default: return 0;
	}
}

void AArenaGameState::SetMatchPhase(EArenaMatchPhase NewPhase, float Duration)
{
	check(HasAuthority());
	MatchPhase = NewPhase;
	PhaseEndTime = Duration > 0.f ? GetServerWorldTimeSeconds() + Duration : 0.0;
}

void AArenaGameState::AddCrowns(EArenaTeam Team, int32 Count)
{
	check(HasAuthority());
	if (Team == EArenaTeam::J)
	{
		CrownsJ += Count;
	}
	else if (Team == EArenaTeam::A)
	{
		CrownsA += Count;
	}
}

void AArenaGameState::SetWinner(EArenaTeam NewWinner)
{
	check(HasAuthority());
	Winner = NewWinner;
}

void AArenaGameState::SetDoubleElixir(bool bEnabled)
{
	check(HasAuthority());
	bDoubleElixir = bEnabled;
}

void AArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaGameState, MatchPhase);
	DOREPLIFETIME(AArenaGameState, PhaseEndTime);
	DOREPLIFETIME(AArenaGameState, CrownsJ);
	DOREPLIFETIME(AArenaGameState, CrownsA);
	DOREPLIFETIME(AArenaGameState, Winner);
	DOREPLIFETIME(AArenaGameState, bDoubleElixir);
}

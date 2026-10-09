#include "ArenaGameMode.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "GameFramework/GameStateBase.h"

AArenaGameMode::AArenaGameMode()
{
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	// Pas de personnage à diriger : le joueur pose des cartes depuis une caméra fixe.
	DefaultPawnClass = nullptr;
}

void AArenaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AArenaPlayerState* NewState = NewPlayer->GetPlayerState<AArenaPlayerState>();
	if (!NewState)
	{
		return;
	}

	// Le premier joueur connecté (l'hôte) prend le camp J, le suivant le camp A.
	bool bTeamJTaken = false;
	for (const APlayerState* Other : GameState->PlayerArray)
	{
		const AArenaPlayerState* OtherState = Cast<AArenaPlayerState>(Other);
		if (OtherState && OtherState != NewState && OtherState->GetTeam() == EArenaTeam::J)
		{
			bTeamJTaken = true;
			break;
		}
	}
	NewState->SetTeam(bTeamJTaken ? EArenaTeam::A : EArenaTeam::J);
}

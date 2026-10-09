#include "ArenaGameMode.h"
#include "Anera.h"
#include "ArenaAIOpponent.h"
#include "ArenaBuilding.h"
#include "ArenaCardData.h"
#include "ArenaGameState.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"

AArenaGameMode::AArenaGameMode()
{
	GameStateClass = AArenaGameState::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	// Pas de personnage à diriger : le joueur pose des cartes depuis une caméra fixe.
	DefaultPawnClass = nullptr;

	BuildingClass = AArenaBuilding::StaticClass();
	AIOpponentClass = AArenaAIOpponent::StaticClass();
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

bool AArenaGameMode::TryPlayCard(AArenaPlayerState* Player, UArenaCardData* Card, const FVector& Location)
{
	UE_LOG(LogArene, Warning, TEXT("TryPlayCard(%s, %s, %s) : pose des cartes pas encore implementee (1.2.4)."),
		*GetNameSafe(Player), *GetNameSafe(Card), *Location.ToCompactString());
	return false;
}

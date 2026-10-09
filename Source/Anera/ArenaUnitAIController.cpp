#include "ArenaUnitAIController.h"
#include "ArenaUnit.h"

AArenaUnitAIController::AArenaUnitAIController()
{
	// Le contrôleur ne sert qu'à la logique serveur : pas besoin de PlayerState.
	bWantsPlayerState = false;
}

AArenaUnit* AArenaUnitAIController::GetUnit() const
{
	return Cast<AArenaUnit>(GetPawn());
}

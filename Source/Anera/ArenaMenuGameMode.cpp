#include "ArenaMenuGameMode.h"
#include "ArenaMenuPlayerController.h"

AArenaMenuGameMode::AArenaMenuGameMode()
{
	PlayerControllerClass = AArenaMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

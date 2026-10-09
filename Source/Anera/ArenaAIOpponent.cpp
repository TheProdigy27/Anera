#include "ArenaAIOpponent.h"
#include "ArenaPlayerState.h"

AArenaAIOpponent::AArenaAIOpponent()
{
	// Logique serveur uniquement : rien à envoyer aux clients.
	bReplicates = false;
}

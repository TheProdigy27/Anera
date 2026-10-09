#include "ArenaAttackComponent.h"

UArenaAttackComponent::UArenaAttackComponent()
{
	// Le tick servira à la cadence de tir (1.2.5) ; inutile tant que la boucle d'attaque n'existe pas.
	PrimaryComponentTick.bCanEverTick = false;
}

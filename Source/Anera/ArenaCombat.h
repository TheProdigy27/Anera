#pragma once

#include "CoreMinimal.h"
#include "ArenaTypes.h"

class AActor;
class UArenaHealthComponent;
class UWorld;

// Règles de combat communes aux unités, bâtiments, projectiles et sorts : camps, ciblage, distances, dégâts.
// Les fonctions qui infligent des dégâts ou des effets ne doivent être appelées que sur le serveur.
namespace ArenaCombat
{
	// Camp d'une unité ou d'un bâtiment (Aucun pour tout autre acteur).
	EArenaTeam GetTeam(const AActor* Actor);

	UArenaHealthComponent* GetHealth(const AActor* Actor);

	// Unité ou bâtiment encore debout.
	bool IsAlive(const AActor* Actor);

	bool IsFlying(const AActor* Actor);

	// Une attaque de ce type peut-elle viser cet acteur ?
	bool CanTarget(EArenaTargetType TargetType, const AActor* Target);

	// Distance entre les bords des deux acteurs (rayon des unités, murs des bâtiments), à plat.
	float GetEdgeDistance(const AActor* From, const AActor* To);

	// Point visé par les projectiles.
	FVector GetAimPoint(const AActor* Target);

	// Ennemi vivant le plus proche de Seeker à moins de MaxDistance (de bord à bord), selon le type de cible.
	AActor* FindNearestEnemy(const AActor* Seeker, EArenaTargetType TargetType, float MaxDistance, bool bIncludeBuildings = true);

	// Serveur : inflige des dégâts à une unité ou un bâtiment.
	void DealDamage(AActor* Target, float Damage, AActor* Causer);

	// Serveur : dégâts à tous les ennemis de AttackerTeam dans le cercle. Les bâtiments ne reçoivent
	// que BuildingDamageScale × Damage.
	void ApplyAreaDamage(UWorld* World, const FVector& Center, float Radius, float Damage, EArenaTeam AttackerTeam,
		EArenaTargetType TargetType, float BuildingDamageScale, AActor* Causer);

	// Serveur : ralentit les unités ennemies dans le cercle (SlowAmount = 0,5 : 50 % plus lentes).
	void ApplyAreaSlow(UWorld* World, const FVector& Center, float Radius, EArenaTeam AttackerTeam, float SlowAmount, float Duration);
}

#pragma once

#include "CoreMinimal.h"
#include "ArenaTypes.h"

class AActor;
class UArenaCardData;

// Lecture des repères posés par le script d'import, partagée par le serveur et les clients.
// Tout passe par les tags des acteurs, de la forme « clé:valeur » (ex. « equipe:joueur », « type:tour »),
// jamais par des coordonnées en dur.
namespace ArenaLevel
{
	// Valeur du premier tag « Key:… » de l'acteur (chaîne vide s'il n'y en a pas). Key s'écrit sans les deux-points.
	FString GetTagValue(const AActor* Actor, const TCHAR* Key);

	// Camp indiqué par le tag « equipe:joueur|adverse ».
	EArenaTeam GetTagTeam(const AActor* Actor);

	EArenaTeam GetOpponent(EArenaTeam Team);

	// Zone de déploiement du camp (TriggerBox taguée « zone » et « type:zone_deploiement »).
	AActor* FindDeploymentZone(const UObject* WorldContext, EArenaTeam Team);

	// Le point est-il au-dessus de la zone de déploiement du camp ? La hauteur est ignorée.
	bool IsInDeploymentZone(const UObject* WorldContext, EArenaTeam Team, const FVector& Point);

	// Point de la NavMesh le plus proche du point visé, cherché dans une boîte de demi-taille Extent.
	// La tolérance par défaut est faible (un clic sur un mur reste refusé) ; les déplacements en prennent une plus large.
	// Faux si aucun point marchable n'est assez proche.
	bool ProjectToNavMesh(const UObject* WorldContext, const FVector& Point, FVector& OutLocation, const FVector& Extent = FVector(40.f, 40.f, 150.f));

	// Un point de la NavMesh est-il relié au départ des chemins du camp ? Écarte les îlots de NavMesh
	// d'où une unité ne pourrait pas descendre (haut des tours, socles).
	bool IsConnectedToLanes(const UObject* WorldContext, EArenaTeam Team, const FVector& NavLocation);

	// Placement d'une carte : les unités dans la zone de leur camp, sur la NavMesh reliée au terrain,
	// les sorts n'importe où. OutLocation reçoit le point retenu, au sol.
	bool IsValidPlacement(const UObject* WorldContext, EArenaTeam Team, const UArenaCardData* Card, const FVector& Point, FVector& OutLocation);

	// Direction d'attaque du camp, à plat : de son fort vers le fort adverse.
	FVector GetAttackDirection(const UObject* WorldContext, EArenaTeam Team);

	// Points du chemin d'une voie (« gauche » ou « droite ») pour les unités du camp, dans l'ordre.
	TArray<FVector> GetLanePath(const UObject* WorldContext, EArenaTeam Team, const FString& Lane);
}

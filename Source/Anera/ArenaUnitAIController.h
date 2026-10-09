#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ArenaUnitAIController.generated.h"

class AArenaUnit;
class UArenaAttackComponent;

// Cerveau d'une unité, sans Behavior Tree. Plusieurs fois par seconde :
//  - choisir une cible : l'ennemi le plus proche dans le rayon de vision, selon le type de cible ;
//    une cible déjà à portée est gardée jusqu'à sa mort ;
//  - à portée : s'arrêter (le composant d'attaque tire à sa cadence) ; sinon : s'en approcher ;
//  - sans cible : suivre le chemin de sa voie (CHEMIN_IA_<camp>_<voie>), choisie à l'apparition
//    selon le côté du terrain.
// Un AIController n'existe que sur le serveur.
UCLASS()
class ANERA_API AArenaUnitAIController : public AAIController
{
	GENERATED_BODY()

public:
	AArenaUnitAIController();

	UFUNCTION(BlueprintPure, Category = "Arene|IA")
	AArenaUnit* GetUnit() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Temps entre deux décisions.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|IA", meta = (ClampMin = "0.05", Units = "s"))
	float DecisionInterval = 0.15f;

	// Une cible qui s'éloigne au-delà de (rayon de vision × ce facteur) est abandonnée.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|IA", meta = (ClampMin = "1"))
	float LeashFactor = 1.5f;

	// Distance à laquelle un point du chemin compte comme atteint.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|IA", meta = (ClampMin = "10", Units = "cm"))
	float WaypointReachDistance = 100.f;

private:
	void UpdateBehavior();
	AActor* SelectTarget(const AArenaUnit* Unit, const UArenaAttackComponent* Attack) const;
	void MoveTowardTarget(const AArenaUnit* Unit, const AActor* Target, float Range);
	void FollowLane(const AArenaUnit* Unit);

	// Voie la plus proche de l'unité, et premier point du chemin encore devant elle.
	void ChooseLane(const AArenaUnit* Unit);
	void RefreshWaypointIndex(const AArenaUnit* Unit);

	// Lance un déplacement, sauf si on se dirige déjà à peu près vers ce point.
	void IssueMove(const AArenaUnit* Unit, FVector Destination, float AcceptanceRadius);

	// Diagnostic : signale (log LogArene, niveau Verbose) une unité qui veut avancer mais reste sur place.
	void CheckProgress(const AArenaUnit* Unit, const AActor* Target);
	void ResetProgress(const AArenaUnit* Unit);

	FVector ProgressLocation = FVector::ZeroVector;
	double ProgressTime = 0.0;
	// Résultat de la dernière demande de déplacement (EPathFollowingRequestResult), pour le diagnostic.
	int32 LastMoveResult = 0;

	TArray<FVector> LanePath;
	int32 WaypointIndex = 0;
	FVector AttackDirection = FVector::ForwardVector;
	bool bFollowingLane = false;
	FVector LastMoveDestination = FVector(TNumericLimits<double>::Max());
	FTimerHandle DecisionTimer;
};

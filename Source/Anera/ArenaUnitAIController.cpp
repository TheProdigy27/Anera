#include "ArenaUnitAIController.h"
#include "ArenaAttackComponent.h"
#include "ArenaBuilding.h"
#include "ArenaCardData.h"
#include "ArenaCombat.h"
#include "ArenaGameState.h"
#include "ArenaLevel.h"
#include "ArenaUnit.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

AArenaUnitAIController::AArenaUnitAIController()
{
	// Le contrôleur ne sert qu'à la logique serveur : pas besoin de PlayerState.
	bWantsPlayerState = false;
}

AArenaUnit* AArenaUnitAIController::GetUnit() const
{
	return Cast<AArenaUnit>(GetPawn());
}

void AArenaUnitAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	const AArenaUnit* Unit = GetUnit();
	if (!Unit)
	{
		return;
	}
	AttackDirection = ArenaLevel::GetAttackDirection(this, Unit->GetTeam());
	ChooseLane(Unit);

	// Premier passage décalé au hasard : les unités ne réfléchissent pas toutes à la même image.
	GetWorldTimerManager().SetTimer(DecisionTimer, this, &AArenaUnitAIController::UpdateBehavior,
		DecisionInterval, true, FMath::FRandRange(0.f, DecisionInterval));
}

void AArenaUnitAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(DecisionTimer);
	Super::OnUnPossess();
}

void AArenaUnitAIController::UpdateBehavior()
{
	const AArenaUnit* Unit = GetUnit();
	if (!Unit || !ArenaCombat::IsAlive(Unit))
	{
		return;
	}
	UArenaAttackComponent* Attack = Unit->GetAttackComponent();

	const AArenaGameState* GameState = GetWorld()->GetGameState<AArenaGameState>();
	if (!GameState || !GameState->IsMatchRunning())
	{
		Attack->SetTarget(nullptr);
		StopMovement();
		return;
	}
	if (!Unit->IsDeployed())
	{
		return;
	}

	AActor* Target = SelectTarget(Unit, Attack);
	Attack->SetTarget(Target);
	if (Target)
	{
		bFollowingLane = false;
		if (Attack->IsTargetInRange())
		{
			StopMovement();
			LastMoveDestination = FVector(TNumericLimits<double>::Max());
			// Face à la cible : le personnage ne se tourne tout seul que lorsqu'il marche.
			const FVector ToTarget = Target->GetActorLocation() - Unit->GetActorLocation();
			GetPawn()->SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
		}
		else
		{
			MoveTowardTarget(Unit, Target, Attack->GetStats().Range);
		}
		return;
	}

	if (!bFollowingLane)
	{
		// On reprend le chemin là où l'unité se trouve, pas forcément là où elle l'avait quitté.
		RefreshWaypointIndex(Unit);
		bFollowingLane = true;
		LastMoveDestination = FVector(TNumericLimits<double>::Max());
	}
	FollowLane(Unit);
}

AActor* AArenaUnitAIController::SelectTarget(const AArenaUnit* Unit, const UArenaAttackComponent* Attack) const
{
	const EArenaTargetType TargetType = Attack->GetStats().TargetType;
	const float SightRange = Unit->GetCard() ? Unit->GetCard()->SightRange : 600.f;

	AActor* Current = Attack->GetTarget();
	const bool bCurrentValid = ArenaCombat::IsAlive(Current) && ArenaCombat::CanTarget(TargetType, Current)
		&& ArenaCombat::GetEdgeDistance(Unit, Current) <= SightRange * LeashFactor;

	// Une cible à portée est gardée jusqu'à sa mort ; sinon, on reprend l'ennemi le plus proche.
	if (bCurrentValid && Attack->IsInRange(Current))
	{
		return Current;
	}
	AActor* Nearest = ArenaCombat::FindNearestEnemy(Unit, TargetType, SightRange);
	return Nearest ? Nearest : (bCurrentValid ? Current : nullptr);
}

void AArenaUnitAIController::MoveTowardTarget(const AArenaUnit* Unit, const AActor* Target, float Range)
{
	FVector Destination = Target->GetActorLocation();
	if (const AArenaBuilding* Building = Cast<AArenaBuilding>(Target))
	{
		// Un bâtiment est un obstacle : on vise le pied du mur le plus proche, à mi-portée.
		const FVector Wall = Building->GetClosestWallPoint(Unit->GetActorLocation());
		const FVector Away = (Unit->GetActorLocation() - Wall).GetSafeNormal2D();
		Destination = Wall + Away * (Unit->GetRadius() + Range * 0.5f);
	}
	IssueMove(Unit, Destination, FMath::Max(10.f, Range * 0.5f));
}

void AArenaUnitAIController::FollowLane(const AArenaUnit* Unit)
{
	while (LanePath.IsValidIndex(WaypointIndex)
		&& FVector::Dist2D(Unit->GetActorLocation(), LanePath[WaypointIndex]) <= WaypointReachDistance)
	{
		++WaypointIndex;
	}
	if (!LanePath.IsValidIndex(WaypointIndex))
	{
		StopMovement();
		return;
	}
	IssueMove(Unit, LanePath[WaypointIndex], WaypointReachDistance * 0.5f);
}

void AArenaUnitAIController::ChooseLane(const AArenaUnit* Unit)
{
	const FVector Location = Unit->GetActorLocation();
	float BestDistance = TNumericLimits<float>::Max();
	for (const TCHAR* Lane : { TEXT("gauche"), TEXT("droite") })
	{
		TArray<FVector> Path = ArenaLevel::GetLanePath(this, Unit->GetTeam(), Lane);
		for (const FVector& Point : Path)
		{
			const float Distance = FVector::Dist2D(Location, Point);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				LanePath = Path;
			}
		}
	}
	RefreshWaypointIndex(Unit);
}

void AArenaUnitAIController::RefreshWaypointIndex(const AArenaUnit* Unit)
{
	// Premier point du chemin situé devant l'unité, dans le sens de l'attaque.
	const FVector Location = Unit->GetActorLocation();
	WaypointIndex = LanePath.Num();
	for (int32 Index = 0; Index < LanePath.Num(); ++Index)
	{
		if (FVector::DotProduct((LanePath[Index] - Location).GetSafeNormal2D(), AttackDirection) > 0.f
			&& FVector::Dist2D(Location, LanePath[Index]) > WaypointReachDistance)
		{
			WaypointIndex = Index;
			break;
		}
	}
}

void AArenaUnitAIController::IssueMove(const AArenaUnit* Unit, FVector Destination, float AcceptanceRadius)
{
	const bool bMoving = GetMoveStatus() != EPathFollowingStatus::Idle;
	if (bMoving && FVector::DistSquared2D(Destination, LastMoveDestination) < FMath::Square(30.f))
	{
		return;
	}
	LastMoveDestination = Destination;

	if (Unit->IsFlying())
	{
		// En vol : ligne droite, à l'altitude actuelle.
		Destination.Z = Unit->GetActorLocation().Z;
		MoveToLocation(Destination, AcceptanceRadius, false, false, false, true, nullptr, true);
	}
	else
	{
		MoveToLocation(Destination, AcceptanceRadius, false, true, true, false, nullptr, true);
	}
}

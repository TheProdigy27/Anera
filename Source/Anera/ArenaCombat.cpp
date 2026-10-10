#include "ArenaCombat.h"
#include "ArenaBuilding.h"
#include "ArenaHealthComponent.h"
#include "ArenaUnit.h"
#include "EngineUtils.h"

namespace ArenaCombat
{
	EArenaTeam GetTeam(const AActor* Actor)
	{
		if (const AArenaUnit* Unit = Cast<AArenaUnit>(Actor))
		{
			return Unit->GetTeam();
		}
		if (const AArenaBuilding* Building = Cast<AArenaBuilding>(Actor))
		{
			return Building->GetTeam();
		}
		return EArenaTeam::None;
	}

	UArenaHealthComponent* GetHealth(const AActor* Actor)
	{
		if (const AArenaUnit* Unit = Cast<AArenaUnit>(Actor))
		{
			return Unit->GetHealthComponent();
		}
		if (const AArenaBuilding* Building = Cast<AArenaBuilding>(Actor))
		{
			return Building->GetHealthComponent();
		}
		return nullptr;
	}

	bool IsAlive(const AActor* Actor)
	{
		const UArenaHealthComponent* Health = IsValid(Actor) ? GetHealth(Actor) : nullptr;
		return Health && !Health->IsDead();
	}

	bool IsFlying(const AActor* Actor)
	{
		const AArenaUnit* Unit = Cast<AArenaUnit>(Actor);
		return Unit && Unit->IsFlying();
	}

	bool CanTarget(EArenaTargetType TargetType, const AActor* Target)
	{
		if (Cast<AArenaBuilding>(Target))
		{
			return TargetType != EArenaTargetType::Air;
		}
		if (!Cast<AArenaUnit>(Target) || TargetType == EArenaTargetType::BuildingsOnly)
		{
			return false;
		}
		return IsFlying(Target)
			? TargetType == EArenaTargetType::Air || TargetType == EArenaTargetType::GroundAndAir
			: TargetType == EArenaTargetType::Ground || TargetType == EArenaTargetType::GroundAndAir;
	}

	float GetEdgeDistance(const AActor* From, const AActor* To)
	{
		if (!From || !To)
		{
			return TNumericLimits<float>::Max();
		}
		// Un bâtiment se mesure toujours par ses murs, qu'il vise ou qu'il soit visé : la distance est la même
		// dans les deux sens. Sinon une tour, mesurée depuis son centre, serait hors de portée d'une unité
		// qui, elle, l'atteint.
		if (Cast<AArenaBuilding>(From) && !Cast<AArenaBuilding>(To))
		{
			Swap(From, To);
		}
		const FVector FromLocation = From->GetActorLocation();
		const AArenaUnit* FromUnit = Cast<AArenaUnit>(From);
		const float FromRadius = FromUnit ? FromUnit->GetRadius() : 0.f;

		float Distance;
		if (const AArenaBuilding* Building = Cast<AArenaBuilding>(To))
		{
			Distance = Building->GetDistanceToWalls(FromLocation);
		}
		else if (const AArenaUnit* Unit = Cast<AArenaUnit>(To))
		{
			Distance = FVector::Dist2D(FromLocation, Unit->GetActorLocation()) - Unit->GetRadius();
		}
		else
		{
			Distance = FVector::Dist2D(FromLocation, To->GetActorLocation());
		}
		return FMath::Max(0.f, Distance - FromRadius);
	}

	FVector GetAimPoint(const AActor* Target)
	{
		if (const AArenaBuilding* Building = Cast<AArenaBuilding>(Target))
		{
			return Building->GetAimPoint();
		}
		return Target ? Target->GetActorLocation() : FVector::ZeroVector;
	}

	AActor* FindNearestEnemy(const AActor* Seeker, EArenaTargetType TargetType, float MaxDistance, bool bIncludeBuildings)
	{
		UWorld* World = Seeker ? Seeker->GetWorld() : nullptr;
		if (!World)
		{
			return nullptr;
		}
		const EArenaTeam Team = GetTeam(Seeker);

		AActor* Best = nullptr;
		float BestDistance = MaxDistance;
		auto Consider = [&](AActor* Candidate)
		{
			if (Candidate == Seeker || GetTeam(Candidate) == Team || !IsAlive(Candidate) || !CanTarget(TargetType, Candidate))
			{
				return;
			}
			const float Distance = GetEdgeDistance(Seeker, Candidate);
			if (Distance <= BestDistance)
			{
				Best = Candidate;
				BestDistance = Distance;
			}
		};

		if (TargetType != EArenaTargetType::BuildingsOnly)
		{
			for (TActorIterator<AArenaUnit> It(World); It; ++It)
			{
				Consider(*It);
			}
		}
		if (bIncludeBuildings)
		{
			for (TActorIterator<AArenaBuilding> It(World); It; ++It)
			{
				Consider(*It);
			}
		}
		return Best;
	}

	void DealDamage(AActor* Target, float Damage, AActor* Causer)
	{
		if (UArenaHealthComponent* Health = IsValid(Target) ? GetHealth(Target) : nullptr)
		{
			Health->ApplyDamage(Damage, Causer);
		}
	}

	void ApplyAreaDamage(UWorld* World, const FVector& Center, float Radius, float Damage, EArenaTeam AttackerTeam,
		EArenaTargetType TargetType, float BuildingDamageScale, AActor* Causer)
	{
		if (!World || Damage <= 0.f)
		{
			return;
		}

		// On liste d'abord les cibles, puis on frappe : une cible peut être détruite pendant la boucle.
		TArray<TPair<AActor*, float>> Hits;
		for (TActorIterator<AArenaUnit> It(World); It; ++It)
		{
			AArenaUnit* Unit = *It;
			if (Unit->GetTeam() != AttackerTeam && IsAlive(Unit) && CanTarget(TargetType, Unit)
				&& FVector::Dist2D(Center, Unit->GetActorLocation()) - Unit->GetRadius() <= Radius)
			{
				Hits.Emplace(Unit, Damage);
			}
		}
		for (TActorIterator<AArenaBuilding> It(World); It; ++It)
		{
			AArenaBuilding* Building = *It;
			if (Building->GetTeam() != AttackerTeam && IsAlive(Building) && CanTarget(TargetType, Building)
				&& Building->GetDistanceToWalls(Center) <= Radius)
			{
				Hits.Emplace(Building, Damage * BuildingDamageScale);
			}
		}
		for (const TPair<AActor*, float>& Hit : Hits)
		{
			DealDamage(Hit.Key, Hit.Value, Causer);
		}
	}

	void ApplyAreaSlow(UWorld* World, const FVector& Center, float Radius, EArenaTeam AttackerTeam, float SlowAmount, float Duration)
	{
		if (!World || SlowAmount <= 0.f || Duration <= 0.f)
		{
			return;
		}
		for (TActorIterator<AArenaUnit> It(World); It; ++It)
		{
			AArenaUnit* Unit = *It;
			if (Unit->GetTeam() != AttackerTeam && IsAlive(Unit)
				&& FVector::Dist2D(Center, Unit->GetActorLocation()) - Unit->GetRadius() <= Radius)
			{
				Unit->ApplySlow(SlowAmount, Duration);
			}
		}
	}
}

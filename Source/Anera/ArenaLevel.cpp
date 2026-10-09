#include "ArenaLevel.h"
#include "ArenaCardData.h"
#include "Components/BoxComponent.h"
#include "Engine/TriggerBox.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

namespace ArenaLevel
{
	FString GetTagValue(const AActor* Actor, const TCHAR* Key)
	{
		if (!Actor)
		{
			return FString();
		}
		const FString Prefix = FString(Key) + TEXT(":");
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(Prefix))
			{
				return TagString.Mid(Prefix.Len());
			}
		}
		return FString();
	}

	EArenaTeam GetTagTeam(const AActor* Actor)
	{
		const FString Value = GetTagValue(Actor, TEXT("equipe"));
		if (Value == TEXT("joueur"))
		{
			return EArenaTeam::J;
		}
		if (Value == TEXT("adverse"))
		{
			return EArenaTeam::A;
		}
		return EArenaTeam::None;
	}

	EArenaTeam GetOpponent(EArenaTeam Team)
	{
		switch (Team)
		{
		case EArenaTeam::J: return EArenaTeam::A;
		case EArenaTeam::A: return EArenaTeam::J;
		default: return EArenaTeam::None;
		}
	}

	AActor* FindDeploymentZone(const UObject* WorldContext, EArenaTeam Team)
	{
		TArray<AActor*> Zones;
		UGameplayStatics::GetAllActorsWithTag(WorldContext, TEXT("zone"), Zones);
		for (AActor* Zone : Zones)
		{
			if (GetTagValue(Zone, TEXT("type")) == TEXT("zone_deploiement") && GetTagTeam(Zone) == Team)
			{
				return Zone;
			}
		}
		return nullptr;
	}

	bool IsInDeploymentZone(const UObject* WorldContext, EArenaTeam Team, const FVector& Point)
	{
		const AActor* Zone = FindDeploymentZone(WorldContext, Team);
		if (!Zone)
		{
			return false;
		}

		// Le point est ramené dans le repère de la boîte : ça marche aussi si la zone est tournée.
		const ATriggerBox* TriggerBox = Cast<ATriggerBox>(Zone);
		if (const UBoxComponent* Box = TriggerBox ? Cast<UBoxComponent>(TriggerBox->GetCollisionComponent()) : nullptr)
		{
			const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Point);
			const FVector Extent = Box->GetUnscaledBoxExtent();
			return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y;
		}

		const FBox Bounds = Zone->GetComponentsBoundingBox(true);
		return Point.X >= Bounds.Min.X && Point.X <= Bounds.Max.X && Point.Y >= Bounds.Min.Y && Point.Y <= Bounds.Max.Y;
	}

	bool ProjectToNavMesh(const UObject* WorldContext, const FVector& Point, FVector& OutLocation)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!NavSystem)
		{
			return false;
		}
		// Petite tolérance à plat (un clic sur un mur reste refusé), plus grande en hauteur (le sol peut être en pente).
		FNavLocation NavLocation;
		if (!NavSystem->ProjectPointToNavigation(Point, NavLocation, FVector(40.f, 40.f, 150.f)))
		{
			return false;
		}
		OutLocation = NavLocation.Location;
		return true;
	}

	bool IsValidPlacement(const UObject* WorldContext, EArenaTeam Team, const UArenaCardData* Card, const FVector& Point, FVector& OutLocation)
	{
		if (!Card)
		{
			return false;
		}
		if (Card->CardType == EArenaCardType::Spell)
		{
			OutLocation = Point;
			return true;
		}
		return IsInDeploymentZone(WorldContext, Team, Point) && ProjectToNavMesh(WorldContext, Point, OutLocation);
	}

	FVector GetAttackDirection(const UObject* WorldContext, EArenaTeam Team)
	{
		TArray<AActor*> Slots;
		UGameplayStatics::GetAllActorsWithTag(WorldContext, TEXT("slot"), Slots);
		const AActor* OwnFort = nullptr;
		const AActor* EnemyFort = nullptr;
		for (const AActor* Slot : Slots)
		{
			if (GetTagValue(Slot, TEXT("type")) != TEXT("fort"))
			{
				continue;
			}
			const EArenaTeam SlotTeam = GetTagTeam(Slot);
			if (SlotTeam == Team)
			{
				OwnFort = Slot;
			}
			else if (SlotTeam == GetOpponent(Team))
			{
				EnemyFort = Slot;
			}
		}
		if (!OwnFort || !EnemyFort)
		{
			return FVector::ForwardVector;
		}
		return (EnemyFort->GetActorLocation() - OwnFort->GetActorLocation()).GetSafeNormal2D();
	}

	TArray<FVector> GetLanePath(const UObject* WorldContext, EArenaTeam Team, const FString& Lane)
	{
		TArray<AActor*> Points;
		UGameplayStatics::GetAllActorsWithTag(WorldContext, TEXT("chemin"), Points);

		TArray<TPair<int32, FVector>> Indexed;
		for (const AActor* Point : Points)
		{
			if (GetTagTeam(Point) == Team && GetTagValue(Point, TEXT("voie")) == Lane)
			{
				Indexed.Emplace(FCString::Atoi(*GetTagValue(Point, TEXT("index"))), Point->GetActorLocation());
			}
		}
		Indexed.Sort([](const TPair<int32, FVector>& A, const TPair<int32, FVector>& B) { return A.Key < B.Key; });

		TArray<FVector> Path;
		for (const TPair<int32, FVector>& Entry : Indexed)
		{
			Path.Add(Entry.Value);
		}
		return Path;
	}
}

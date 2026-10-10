#include "Anera.h"
#include "ArenaUnit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

// Mesure de la fluidité des déplacements, pour les tests réseau (tâches 2.3 et 2.6 du plan).
//
// « Arene.MesureMouvement [secondes] » relève à chaque image, dans chaque monde du processus (en PIE : le
// serveur et chaque client), deux positions par unité :
// - la capsule : chez un client, elle est recalée d'un coup à chaque position reçue du serveur ;
// - le visuel : CharacterMovement le lisse chez les clients (sur le serveur, il est confondu avec la capsule).
// Elle compte ensuite les à-coups : les images où l'accélération apparente dépasse de loin ce qu'une unité
// peut faire d'elle-même (CharacterMovement : 2048 uu/s² au plus). Le serveur sert de référence.
namespace
{
	// Accélération apparente au-delà de laquelle on compte un à-coup.
	constexpr double JerkThreshold = 6000.0;

	// Un point suivi d'image en image : position et vitesse à l'image précédente.
	struct FPointTrack
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		int32 Samples = 0;
	};

	struct FMotionStats
	{
		int64 Frames = 0;
		int64 Jerks = 0;
		double MaxAcceleration = 0.0;

		void Add(FPointTrack& Track, const FVector& Position, double DeltaTime)
		{
			const FVector Velocity = (Position - Track.Position) / DeltaTime;
			// Il faut deux images de recul pour avoir une vitesse fiable, puis une accélération.
			if (Track.Samples >= 2)
			{
				const double Acceleration = (Velocity - Track.Velocity).Size() / DeltaTime;
				++Frames;
				Jerks += Acceleration > JerkThreshold ? 1 : 0;
				MaxAcceleration = FMath::Max(MaxAcceleration, Acceleration);
			}
			Track.Position = Position;
			Track.Velocity = Velocity;
			++Track.Samples;
		}

		FString Describe() const
		{
			return FString::Printf(TEXT("%lld a-coups sur %lld images-unites (%.2f %%), acceleration max %.0f uu/s2"),
				Jerks, Frames, Frames > 0 ? 100.0 * Jerks / Frames : 0.0, MaxAcceleration);
		}
	};

	struct FUnitTracks
	{
		FPointTrack Capsule;
		FPointTrack Visual;
	};

	// Mesure en cours dans un monde.
	struct FWorldMeasure
	{
		TWeakObjectPtr<UWorld> World;
		double EndTime = 0.0;
		double MeasuredTime = 0.0;
		int64 Frames = 0;
		TMap<TWeakObjectPtr<AArenaUnit>, FUnitTracks> Units;
		FMotionStats Capsule;
		FMotionStats Visual;
		double OffsetSum = 0.0;
		double MaxOffset = 0.0;
		int64 OffsetSamples = 0;
	};

	TArray<FWorldMeasure> Measures;
	FDelegateHandle TickHandle;

	const TCHAR* GetMachineName(const UWorld* World)
	{
		return World && World->GetNetMode() == NM_Client ? TEXT("client") : TEXT("serveur");
	}

	void Report(const FWorldMeasure& Measure)
	{
		const UWorld* World = Measure.World.Get();
		UE_LOG(LogArene, Log, TEXT("Arene.MesureMouvement (%s %s, %.1f s, %.0f images/s, %d unites) :"),
			GetMachineName(World), *GetNameSafe(World), Measure.MeasuredTime,
			Measure.MeasuredTime > 0.0 ? Measure.Frames / Measure.MeasuredTime : 0.0, Measure.Units.Num());
		UE_LOG(LogArene, Log, TEXT("  capsule : %s"), *Measure.Capsule.Describe());
		UE_LOG(LogArene, Log, TEXT("  visuel  : %s"), *Measure.Visual.Describe());
		UE_LOG(LogArene, Log, TEXT("  ecart visuel-capsule : moyen %.1f uu, max %.1f uu"),
			Measure.OffsetSamples > 0 ? Measure.OffsetSum / Measure.OffsetSamples : 0.0, Measure.MaxOffset);
	}

	void StopMeasures()
	{
		FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
		TickHandle.Reset();
		Measures.Reset();
	}

	// Après le tick de tous les acteurs d'un monde : le lissage de l'image est appliqué.
	void SampleWorld(UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		FWorldMeasure* Measure = Measures.FindByPredicate([World](const FWorldMeasure& Candidate) { return Candidate.World.Get() == World; });
		if (!Measure || DeltaSeconds <= 0.f)
		{
			return;
		}

		++Measure->Frames;
		Measure->MeasuredTime += DeltaSeconds;
		for (TActorIterator<AArenaUnit> It(World); It; ++It)
		{
			AArenaUnit* Unit = *It;
			FUnitTracks& Tracks = Measure->Units.FindOrAdd(Unit);
			const FVector CapsuleLocation = Unit->GetActorLocation();
			const FVector VisualLocation = Unit->GetVisualLocation();
			Measure->Capsule.Add(Tracks.Capsule, CapsuleLocation, DeltaSeconds);
			Measure->Visual.Add(Tracks.Visual, VisualLocation, DeltaSeconds);

			const double Offset = FVector::Dist(CapsuleLocation, VisualLocation);
			Measure->OffsetSum += Offset;
			Measure->MaxOffset = FMath::Max(Measure->MaxOffset, Offset);
			++Measure->OffsetSamples;
		}

		if (World->GetTimeSeconds() >= Measure->EndTime)
		{
			Report(*Measure);
			Measures.RemoveAll([World](const FWorldMeasure& Candidate) { return Candidate.World.Get() == World; });
		}
		// Les mesures des mondes fermés en cours de route sont abandonnées.
		Measures.RemoveAll([](const FWorldMeasure& Candidate) { return !Candidate.World.IsValid(); });
		if (Measures.IsEmpty())
		{
			StopMeasures();
		}
	}

	void StartMeasures(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		const double Seconds = Args.IsValidIndex(0) ? FMath::Max(1.0, FCString::Atod(*Args[0])) : 10.0;
		StopMeasures();
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World && World->IsGameWorld())
				{
					FWorldMeasure& Measure = Measures.AddDefaulted_GetRef();
					Measure.World = World;
					Measure.EndTime = World->GetTimeSeconds() + Seconds;
				}
			}
		}
		if (Measures.IsEmpty())
		{
			UE_LOG(LogArene, Warning, TEXT("Arene.MesureMouvement : aucun monde de jeu (lancer le PIE d'abord)."));
			return;
		}
		TickHandle = FWorldDelegates::OnWorldPostActorTick.AddStatic(&SampleWorld);
		UE_LOG(LogArene, Log, TEXT("Arene.MesureMouvement : mesure pendant %.0f s dans %d monde(s)."), Seconds, Measures.Num());
	}

	FAutoConsoleCommandWithWorldAndArgs MeasureMotionCommand(
		TEXT("Arene.MesureMouvement"),
		TEXT("Arene.MesureMouvement [secondes] : compte les a-coups des unites (capsule et visuel lisse) sur le serveur ")
		TEXT("et chaque client du processus, puis affiche le resultat dans LogArene."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartMeasures));
}

#include "ArenaGameState.h"
#include "Anera.h"
#include "ArenaEffectMarker.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

float AArenaGameState::GetRemainingTime() const
{
	if (PhaseEndTime <= 0.0)
	{
		return 0.f;
	}
	// GetServerWorldTimeSeconds : l'heure du serveur, estimée chez les clients.
	return static_cast<float>(FMath::Max(0.0, PhaseEndTime - GetServerWorldTimeSeconds()));
}

int32 AArenaGameState::GetCrowns(EArenaTeam Team) const
{
	switch (Team)
	{
	case EArenaTeam::J: return CrownsJ;
	case EArenaTeam::A: return CrownsA;
	default: return 0;
	}
}

void AArenaGameState::SetMatchPhase(EArenaMatchPhase NewPhase, float Duration)
{
	check(HasAuthority());
	MatchPhase = NewPhase;
	PhaseEndTime = Duration > 0.f ? GetServerWorldTimeSeconds() + Duration : 0.0;
}

void AArenaGameState::AddCrowns(EArenaTeam Team, int32 Count)
{
	check(HasAuthority());
	if (Team == EArenaTeam::J)
	{
		CrownsJ += Count;
	}
	else if (Team == EArenaTeam::A)
	{
		CrownsA += Count;
	}
}

void AArenaGameState::SetCrowns(EArenaTeam Team, int32 Count)
{
	AddCrowns(Team, Count - GetCrowns(Team));
}

void AArenaGameState::SetWinner(EArenaTeam NewWinner)
{
	check(HasAuthority());
	Winner = NewWinner;
}

void AArenaGameState::SetDoubleElixir(bool bEnabled)
{
	check(HasAuthority());
	bDoubleElixir = bEnabled;
}

void AArenaGameState::PlayEffect(const FArenaEffect& Effect)
{
	check(HasAuthority());
	PendingEffects.Add(Effect);
	if (!bEffectSendScheduled)
	{
		bEffectSendScheduled = true;
		GetWorldTimerManager().SetTimerForNextTick(this, &AArenaGameState::SendPendingEffects);
	}
}

void AArenaGameState::SendPendingEffects()
{
	bEffectSendScheduled = false;
	if (PendingEffects.IsEmpty())
	{
		return;
	}
	// Un seul RPC pour tous les effets en attente : le moteur ne laisse passer que 2 appels d'un même
	// multicast non fiable par mise à jour réseau d'un acteur (net.MaxRPCPerNetUpdate), les suivants sont perdus.
	const TArray<FArenaEffect> Effects = MoveTemp(PendingEffects);
	PendingEffects.Reset();
	MulticastPlayEffects(Effects);
	// Un multicast non fiable part avec la prochaine mise à jour réseau de l'acteur, et le GameState
	// n'en a que 10 par seconde : on la demande tout de suite pour ne pas retarder les effets.
	ForceNetUpdate();
}

void AArenaGameState::MulticastPlayEffects_Implementation(const TArray<FArenaEffect>& Effects)
{
	for (const FArenaEffect& Effect : Effects)
	{
		UE_LOG(LogArene, VeryVerbose, TEXT("Effet %s en %s (rayon %.0f, %.1f s) sur %s"), *UEnum::GetValueAsString(Effect.Type),
			*Effect.Location.ToCompactString(), Effect.Radius, Effect.Duration, GetWorld()->GetNetMode() == NM_Client ? TEXT("client") : TEXT("serveur"));
		SpawnEffect(Effect);
	}
}

void AArenaGameState::SpawnEffect_Implementation(const FArenaEffect& Effect)
{
	// Un serveur dédié n'affiche rien (pas utilisé pour l'instant, mais rien ne l'empêche).
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const TSubclassOf<AArenaEffectMarker> MarkerClass = EffectMarkerClass ? EffectMarkerClass : TSubclassOf<AArenaEffectMarker>(AArenaEffectMarker::StaticClass());
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AArenaEffectMarker* Marker = GetWorld()->SpawnActor<AArenaEffectMarker>(MarkerClass, FVector(Effect.Location), FRotator::ZeroRotator, SpawnParams))
	{
		Marker->InitializeMarker(Effect);
	}
}

void AArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaGameState, MatchPhase);
	DOREPLIFETIME(AArenaGameState, PhaseEndTime);
	DOREPLIFETIME(AArenaGameState, CrownsJ);
	DOREPLIFETIME(AArenaGameState, CrownsA);
	DOREPLIFETIME(AArenaGameState, Winner);
	DOREPLIFETIME(AArenaGameState, bDoubleElixir);
}

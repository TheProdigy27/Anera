#include "ArenaAttackComponent.h"
#include "ArenaCombat.h"
#include "ArenaGameState.h"
#include "ArenaProjectile.h"
#include "Engine/World.h"

UArenaAttackComponent::UArenaAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UArenaAttackComponent::BeginPlay()
{
	Super::BeginPlay();
	// Les clients ne tirent jamais : seul le serveur a besoin du tick.
	if (!GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
	}
}

void UArenaAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* AttackTarget = Target.Get();
	if (!bAttackEnabled || !ArenaCombat::IsAlive(GetOwner()) || !ArenaCombat::IsAlive(AttackTarget) || !IsInRange(AttackTarget))
	{
		return;
	}
	const AArenaGameState* GameState = GetWorld()->GetGameState<AArenaGameState>();
	if (!GameState || !GameState->IsMatchRunning())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAttackTime)
	{
		return;
	}
	NextAttackTime = Now + Stats.Interval;
	PerformAttack(AttackTarget);
}

bool UArenaAttackComponent::IsInRange(const AActor* Candidate) const
{
	return Candidate && ArenaCombat::GetEdgeDistance(GetOwner(), Candidate) <= Stats.Range;
}

void UArenaAttackComponent::SetProjectileOrigin(const FVector& WorldLocation)
{
	bHasProjectileOrigin = true;
	ProjectileOrigin = WorldLocation;
}

FVector UArenaAttackComponent::GetProjectileOrigin() const
{
	return bHasProjectileOrigin ? ProjectileOrigin : GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 40.f);
}

void UArenaAttackComponent::PerformAttack(AActor* AttackTarget)
{
	AActor* Owner = GetOwner();
	const EArenaTeam Team = ArenaCombat::GetTeam(Owner);

	if (Stats.ProjectileClass)
	{
		const FVector Origin = GetProjectileOrigin();
		const FTransform SpawnTransform((ArenaCombat::GetAimPoint(AttackTarget) - Origin).Rotation(), Origin);
		AArenaProjectile* Projectile = GetWorld()->SpawnActorDeferred<AArenaProjectile>(Stats.ProjectileClass, SpawnTransform,
			Owner, Cast<APawn>(Owner), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Projectile)
		{
			Projectile->InitializeProjectile(AttackTarget, Stats.Damage, Stats.SplashRadius, Stats.TargetType, Team);
			Projectile->FinishSpawning(SpawnTransform);
		}
	}
	else if (Stats.SplashRadius > 0.f)
	{
		ArenaCombat::ApplyAreaDamage(GetWorld(), AttackTarget->GetActorLocation(), Stats.SplashRadius, Stats.Damage, Team,
			Stats.TargetType, 1.f, Owner);
	}
	else
	{
		ArenaCombat::DealDamage(AttackTarget, Stats.Damage, Owner);
	}

	OnAttack.Broadcast(AttackTarget);
}

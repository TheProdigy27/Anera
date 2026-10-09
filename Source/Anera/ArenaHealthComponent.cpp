#include "ArenaHealthComponent.h"
#include "Net/UnrealNetwork.h"

UArenaHealthComponent::UArenaHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Le composant est répliqué avec son acteur : ses variables Replicated arrivent chez les clients.
	SetIsReplicatedByDefault(true);
}

void UArenaHealthComponent::SetMaxHealth(float NewMaxHealth, bool bRefill)
{
	check(GetOwner()->HasAuthority());
	MaxHealth = FMath::Max(1.f, NewMaxHealth);
	const float OldHealth = Health;
	Health = bRefill ? MaxHealth : FMath::Min(Health, MaxHealth);
	if (Health != OldHealth)
	{
		OnHealthChanged.Broadcast(this, OldHealth - Health, nullptr);
	}
}

float UArenaHealthComponent::ApplyDamage(float Damage, AActor* DamageCauser)
{
	check(GetOwner()->HasAuthority());
	if (Damage <= 0.f || IsDead())
	{
		return 0.f;
	}

	const float Applied = FMath::Min(Damage, Health);
	Health -= Applied;
	OnDamaged.Broadcast(this, Applied, DamageCauser);
	OnHealthChanged.Broadcast(this, Applied, DamageCauser);
	if (IsDead())
	{
		OnDied.Broadcast(this, DamageCauser);
	}
	return Applied;
}

void UArenaHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(this, OldHealth - Health, nullptr);
}

void UArenaHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArenaHealthComponent, Health);
	DOREPLIFETIME(UArenaHealthComponent, MaxHealth);
}

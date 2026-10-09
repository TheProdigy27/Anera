#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArenaHealthComponent.generated.h"

class UArenaHealthComponent;

// Événements auxquels le Blueprint ou le C++ peut s'abonner (barres de PV, effets, changement d'état des bâtiments).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FArenaDamagedSignature, UArenaHealthComponent*, HealthComponent, float, Damage, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaDiedSignature, UArenaHealthComponent*, HealthComponent, AActor*, Killer);

// Points de vie d'une unité ou d'un bâtiment.
// Les PV ne changent que sur le serveur (ApplyDamage) puis sont répliqués aux clients.
// OnDamaged et OnDied sont diffusés sur le serveur ; côté client, OnHealthChanged suit la valeur répliquée.
UCLASS(ClassGroup = (Arene), meta = (BlueprintSpawnableComponent))
class ANERA_API UArenaHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaHealthComponent();

	// Serveur uniquement : fixe les PV max. Avec bRefill, les PV actuels repartent au maximum.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arene|Vie")
	void SetMaxHealth(float NewMaxHealth, bool bRefill = true);

	// Serveur uniquement : retire des PV. Renvoie les dégâts réellement appliqués.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arene|Vie")
	float ApplyDamage(float Damage, AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "Arene|Vie")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Arene|Vie")
	float GetMaxHealth() const { return MaxHealth; }

	// PV restants entre 0 et 1.
	UFUNCTION(BlueprintPure, Category = "Arene|Vie")
	float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Arene|Vie")
	bool IsDead() const { return Health <= 0.f; }

	// Serveur : à chaque coup reçu.
	UPROPERTY(BlueprintAssignable, Category = "Arene|Vie")
	FArenaDamagedSignature OnDamaged;

	// Serveur : quand les PV atteignent 0 (une seule fois).
	UPROPERTY(BlueprintAssignable, Category = "Arene|Vie")
	FArenaDiedSignature OnDied;

	// Serveur et clients : à chaque changement de PV (pour l'affichage).
	UPROPERTY(BlueprintAssignable, Category = "Arene|Vie")
	FArenaDamagedSignature OnHealthChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// ReplicatedUsing : chez les clients, OnRep_Health est appelée quand la nouvelle valeur arrive.
	UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleInstanceOnly, Category = "Arene|Vie")
	float Health = 100.f;

	UPROPERTY(Replicated, EditAnywhere, Category = "Arene|Vie", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UFUNCTION()
	void OnRep_Health(float OldHealth);
};

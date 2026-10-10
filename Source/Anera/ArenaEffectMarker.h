#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaTypes.h"
#include "ArenaEffectMarker.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

// Marqueur provisoire d'un effet, en attendant les effets Niagara de la phase 4 : une sphère pour un coup
// porté à une seule cible, un disque au sol pour une zone (dégâts de zone, sort). Un effet instantané
// grandit puis disparaît ; un effet durable (Mur de ronces) reste en place toute sa durée.
// Acteur local, jamais répliqué : chaque machine crée le sien quand l'effet lui parvient (AArenaGameState::SpawnEffect).
UCLASS(Blueprintable)
class ANERA_API AArenaEffectMarker : public AActor
{
	GENERATED_BODY()

public:
	AArenaEffectMarker();

	void InitializeMarker(const FArenaEffect& Effect);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Arene|Effets")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// Forme d'un coup sur une seule cible.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Effets")
	TObjectPtr<UStaticMesh> HitShape;

	// Forme d'une zone, aplatie en disque.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Effets")
	TObjectPtr<UStaticMesh> ZoneShape;

	// Diamètre de la sphère d'un coup sur une seule cible.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Effets", meta = (ClampMin = "1", Units = "cm"))
	float HitSize = 40.f;

	// Durée de vie d'un effet instantané.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Effets", meta = (ClampMin = "0.05", Units = "s"))
	float FlashDuration = 0.25f;

	// Couleur selon le camp qui frappe, plus claire que celle des unités pour rester visible sur elles.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Effets")
	FLinearColor TeamJColor = FLinearColor(0.45f, 0.8f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Effets")
	FLinearColor TeamAColor = FLinearColor(1.f, 0.6f, 0.15f);

private:
	FVector FullScale = FVector::OneVector;
	float Age = 0.f;
};

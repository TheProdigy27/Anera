#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "ArenaHealthBarComponent.generated.h"

class UArenaHealthComponent;

// Barre de PV d'une unité ou d'un bâtiment (widget UArenaHealthBarWidget), sur la machine de chaque joueur.
//
// Un WidgetComponent affiche un widget UMG accroché à un acteur. En espace écran (Screen), le widget est dessiné
// à plat par-dessus la vue, à l'endroit où le composant se projette à l'écran : il garde la même taille quelle que
// soit la distance, et reste toujours face au joueur, même avec la caméra retournée du camp A.
//
// La barre suit le composant de vie de son acteur : OnHealthChanged est diffusé sur le serveur et, chez les
// clients, à chaque PV reçu. Elle disparaît quand les PV tombent à 0 (bâtiment en ruine).
// Le widget est choisi par l'acteur (HealthBarWidgetClass de AArenaUnit et AArenaBuilding) avant son BeginPlay.
UCLASS(ClassGroup = (Arene), meta = (BlueprintSpawnableComponent))
class ANERA_API UArenaHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UArenaHealthBarComponent();

	// Relit les PV et le camp de l'acteur, et met la barre à jour.
	void Refresh();

protected:
	virtual void BeginPlay() override;

	// Barre cachée tant que les PV sont au maximum (moins de barres à l'écran quand beaucoup d'unités sont en jeu).
	UPROPERTY(EditAnywhere, Category = "Arene|Vie")
	bool bHideWhenFull = false;

private:
	UFUNCTION()
	void HandleHealthChanged(UArenaHealthComponent* ChangedHealth, float Damage, AActor* DamageCauser);

	UPROPERTY(Transient)
	TObjectPtr<UArenaHealthComponent> Health;
};

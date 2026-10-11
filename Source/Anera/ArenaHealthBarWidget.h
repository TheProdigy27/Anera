#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaTypes.h"
#include "ArenaHealthBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

// Barre de PV au-dessus d'une unité ou d'un bâtiment, à la couleur de son camp. La mise en page est dans les
// Widget Blueprints enfants : WBP_BarrePV_Unite (barre seule) et WBP_BarrePV_Batiment (barre et PV en chiffres).
// Affichée par un UArenaHealthBarComponent, qui lui transmet les PV à chaque changement.
UCLASS(Abstract)
class ANERA_API UArenaHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// PV et camp à afficher. Les widgets ne sont modifiés que si quelque chose a changé.
	void SetHealth(float Health, float MaxHealth, EArenaTeam Team);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	// PV restants en chiffres. BindWidgetOptional : seule la barre des bâtiments en a.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Vie")
	FLinearColor TeamJColor = FLinearColor(0.15f, 0.45f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Vie")
	FLinearColor TeamAColor = FLinearColor(1.f, 0.2f, 0.15f);

private:
	float ShownHealth = -1.f;
	float ShownMaxHealth = -1.f;
	EArenaTeam ShownTeam = EArenaTeam::None;
	bool bTeamShown = false;
};

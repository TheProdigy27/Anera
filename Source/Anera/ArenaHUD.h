#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaHUD.generated.h"

class UArenaHUDWidget;

// HUD du joueur. Un AHUD existe sur la machine de chaque joueur et dessine directement sur l'écran (Canvas) à chaque image.
// - Il affiche l'interface de la partie (HUDWidgetClass, c'est-à-dire WBP_HUD) : cartes, élixir, chrono, couronnes.
// - Il dessine encore le bandeau de fin de partie, en attendant l'écran de fin (3.4).
// - Débogage, avec la variable de console « Arene.HUDDebug 1 » (ou si HUDWidgetClass n'est pas réglée) : panneau
//   de texte (phase, chrono, élixir, main, PV des bâtiments) et étiquette (nom, PV) au-dessus de chaque unité et bâtiment.
//   Les barres de PV elles-mêmes sont des UArenaHealthBarComponent, sur les unités et les bâtiments.
UCLASS()
class ANERA_API AArenaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;

	// Interface de la partie, créée pour le joueur local au début (réglée dans BP_ArenaHUD : WBP_HUD).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	TSubclassOf<UArenaHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TeamJColor = FLinearColor(0.35f, 0.65f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TeamAColor = FLinearColor(1.f, 0.45f, 0.35f);

	// Fond semi-transparent sous les textes.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor PanelColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);

private:
	void DrawDebugPanel(float Scale);
	void DrawWorldLabels(float Scale);
	void DrawEndBanner(float Scale);

	// Texte centré horizontalement sur ScreenX.
	void DrawCenteredText(const FString& Text, const FLinearColor& Color, float ScreenX, float ScreenY, UFont* Font, float Scale);

	UPROPERTY(Transient)
	TObjectPtr<UArenaHUDWidget> HUDWidget;
};

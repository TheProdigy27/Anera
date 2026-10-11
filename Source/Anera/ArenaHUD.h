#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaHUD.generated.h"

class UArenaHUDWidget;

// HUD du joueur. Un AHUD existe sur la machine de chaque joueur et dessine directement sur l'écran (Canvas) à chaque image.
// - Il affiche l'interface de la partie (HUDWidgetClass, c'est-à-dire WBP_HUD) : cartes, élixir, chrono, couronnes.
// - Il dessine encore, en attendant la suite de la phase 3 : une étiquette (nom, PV) au-dessus de chaque unité et
//   bâtiment (remplacée par les barres de PV en 3.2), et le bandeau de fin de partie (remplacé par l'écran de fin en 3.4).
// - Panneau de débogage (phase, chrono, élixir, main, PV des bâtiments) : variable de console « Arene.HUDDebug 1 ».
//   Il s'affiche aussi si HUDWidgetClass n'est pas réglée.
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

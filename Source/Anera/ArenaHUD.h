#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaHUD.generated.h"

// Affichage de débogage, en attendant la vraie interface (phase 3) : phase et chrono, couronnes,
// élixir, main et carte suivante, PV des bâtiments, et une étiquette (nom, PV) au-dessus de chaque unité.
// Un AHUD existe sur la machine de chaque joueur et dessine directement sur l'écran (Canvas) à chaque image.
UCLASS()
class ANERA_API AArenaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TeamJColor = FLinearColor(0.35f, 0.65f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TeamAColor = FLinearColor(1.f, 0.45f, 0.35f);

	// Fond semi-transparent sous les textes.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor PanelColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);

private:
	void DrawMatchPanel(float Scale);
	void DrawWorldLabels(float Scale);
	void DrawEndBanner(float Scale);

	// Texte centré horizontalement sur ScreenX.
	void DrawCenteredText(const FString& Text, const FLinearColor& Color, float ScreenX, float ScreenY, UFont* Font, float Scale);

	// Adresses de cette machine sur le réseau local, affichées à l'hôte qui attend un adversaire.
	FString LocalAddresses;
};

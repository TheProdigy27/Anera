#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaCardWidget.generated.h"

class UArenaCardData;
class UBorder;
class UImage;
class UTextBlock;

// Une carte affichée par le HUD : icône, nom et coût en élixir. Sert aux 4 cartes de la main et à l'aperçu de
// la carte suivante. La mise en page est dans le Widget Blueprint WBP_Carte (enfant de cette classe).
//
// Une carte de la main se choisit d'un clic, comme avec les touches 1 à 4. Le clic est consommé par l'interface :
// il n'arrive pas au PlayerController, et ne pose donc pas la carte choisie avant.
UCLASS(Abstract)
class ANERA_API UArenaCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Carte affichée. Null : emplacement vide, la carte est masquée.
	void SetCard(UArenaCardData* InCard);

	UArenaCardData* GetCard() const { return Card; }

	// Emplacement dans la main (0 à 3), ou INDEX_NONE pour l'aperçu de la carte suivante, qui ne se clique pas.
	void SetHandIndex(int32 InHandIndex);

	// bSelected : carte choisie (relevée, cadre doré). bPlayable : assez d'élixir (sinon grisée).
	// bPending : envoyée au serveur, en attente de sa réponse (grisée et estompée).
	void SetState(bool bInSelected, bool bInPlayable, bool bInPending);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Cadre de la carte : sa couleur signale la carte choisie.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> CardFrame;

	// Fond de la carte sous l'icône, coloré selon le type (unité ou sort).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> CardFace;

	// Icône de la carte (UArenaCardData::Icon), masquée tant que la carte n'en a pas.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CostText;

	// Touche de l'emplacement (1 à 4). BindWidgetOptional : le Widget Blueprint peut s'en passer.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Carte")
	FLinearColor FrameColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Carte")
	FLinearColor SelectedFrameColor = FLinearColor(1.f, 0.78f, 0.15f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Carte")
	FLinearColor UnitFaceColor = FLinearColor(0.05f, 0.11f, 0.24f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Carte")
	FLinearColor SpellFaceColor = FLinearColor(0.24f, 0.05f, 0.15f);

	// Teinte d'une carte injouable : elle multiplie les couleurs de tout le widget.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Carte")
	FLinearColor UnplayableTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.f);

	// Teinte d'une carte envoyée au serveur, en attente de sa réponse.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Carte")
	FLinearColor PendingTint = FLinearColor(0.4f, 0.4f, 0.4f, 0.5f);

	// Hauteur dont se relève la carte choisie, en pixels de l'interface.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Carte", meta = (ClampMin = "0"))
	float SelectedLift = 14.f;

private:
	void ApplyState();

	UPROPERTY(Transient)
	TObjectPtr<UArenaCardData> Card;

	int32 HandIndex = INDEX_NONE;
	bool bSelected = false;
	bool bPlayable = true;
	bool bPending = false;
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaDeckWidget.generated.h"

class UArenaCardData;
class UArenaCardWidget;
class UButton;
class UTextBlock;
class UWrapBox;

// Écran de choix du deck, ouvert par le bouton « Deck » du menu : toutes les cartes du jeu, et 8 à choisir.
// Un clic sur une carte l'ajoute au deck ou l'en retire (les cartes du deck sont relevées, avec un cadre doré).
// « Enregistrer » garde le deck pour les prochaines parties (UArenaGameInstance::SetDeck, en SaveGame) ;
// « Retour » ou Échap referment l'écran sans rien changer.
// La mise en page est dans le Widget Blueprint WBP_Deck (enfant de cette classe), placé dans WBP_MenuPrincipal.
UCLASS(Abstract)
class ANERA_API UArenaDeckWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Ouvre l'écran avec le deck actuel du joueur.
	void Open();

	// L'écran se ferme : bSaved est vrai si un nouveau deck vient d'être enregistré. Le menu se réaffiche.
	DECLARE_DELEGATE_OneParam(FOnClosed, bool /*bSaved*/);
	FOnClosed OnClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Les cartes du jeu, une WBP_Carte par carte (créées par le C++).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> CardGrid;

	// « 7 / 8 cartes · coût moyen 3,4 »
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CountText;

	// Deck complet, échec de l'enregistrement…
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	// Actif seulement avec 8 cartes.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	// Widget d'une carte (WBP_Carte).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Deck")
	TSubclassOf<UArenaCardWidget> CardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Deck")
	FSlateColor InfoColor = FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f));

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Deck")
	FSlateColor ErrorColor = FSlateColor(FLinearColor(1.f, 0.45f, 0.35f));

private:
	// Crée une carte à l'écran pour chaque carte du jeu.
	void BuildCardGrid();

	// Met à l'écran le deck en cours : cartes choisies, compteur, bouton « Enregistrer ».
	void Refresh();

	void HandleCardClicked(UArenaCardWidget* CardWidget);

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleBackClicked();

	void Close(bool bSaved);
	void ShowMessage(const FText& Message, bool bError);

	// Deck en cours de choix, dans l'ordre des clics (l'ordre ne compte pas : le deck est mélangé en partie).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UArenaCardData>> Selection;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArenaCardWidget>> CardWidgets;
};

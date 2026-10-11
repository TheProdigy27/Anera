#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaTypes.h"
#include "ArenaEndScreenWidget.generated.h"

class AArenaGameState;
class AArenaPlayerState;
class UButton;
class UTextBlock;

// Écran de fin de partie : victoire, défaite ou match nul, couronnes des deux camps, et deux boutons.
// - « Rejouer » demande une revanche. En solo, elle commence tout de suite ; en réseau, quand l'adversaire l'a demandée
//   aussi (l'écran dit qui attend qui). Si l'adversaire est parti, l'hôte rouvre la partie pour un nouvel adversaire.
// - « Menu principal » quitte la partie (comme Échap).
// Il apparaît ShowDelay secondes après la fin, le temps de voir tomber le dernier bâtiment.
//
// La mise en page est dans le Widget Blueprint WBP_FinPartie (enfant de cette classe), placé dans WBP_HUD
// par-dessus le reste. Le HUD le met à jour à chaque image (UpdateEndScreen).
UCLASS(Abstract)
class ANERA_API UArenaEndScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Affiche l'écran quelques instants après la fin de la partie, le cache sinon, et met ses textes à jour.
	void UpdateEndScreen(const AArenaGameState* GameState, const AArenaPlayerState* Player, float DeltaTime);

protected:
	virtual void NativeOnInitialized() override;

	// « Victoire ! », « Défaite » ou « Match nul ».
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultText;

	// Couronnes du joueur et de son adversaire, à la couleur de leur camp.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OwnCrownsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyCrownsText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnCrownsLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnemyCrownsLabel;

	// Où en est la revanche, en réseau : qui attend qui, adversaire parti. Masqué s'il n'y a rien à dire.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RematchButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MenuButton;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Fin")
	FLinearColor VictoryColor = FLinearColor(1.f, 0.78f, 0.25f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Fin")
	FLinearColor DefeatColor = FLinearColor(0.85f, 0.3f, 0.28f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Fin")
	FLinearColor DrawColor = FLinearColor(0.85f, 0.85f, 0.9f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Fin")
	FLinearColor TeamJColor = FLinearColor(0.35f, 0.65f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Fin")
	FLinearColor TeamAColor = FLinearColor(1.f, 0.45f, 0.35f);

	// Délai entre la fin de la partie et l'apparition de l'écran.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Fin", meta = (ClampMin = "0", Units = "s"))
	float ShowDelay = 1.5f;

private:
	UFUNCTION()
	void HandleRematchClicked();

	UFUNCTION()
	void HandleMenuClicked();

	void UpdateResult(const AArenaGameState* GameState, EArenaTeam OwnTeam);
	void UpdateRematchStatus(const AArenaGameState* GameState, const AArenaPlayerState* Player);

	// Change le texte et la couleur d'un widget seulement s'ils diffèrent de ce qui est affiché.
	static void SetTextIfChanged(UTextBlock* TextBlock, const FString& Text, const FLinearColor& Color, FString& Shown);

	float TimeSinceEnd = 0.f;
	bool bShown = false;

	// « Rejouer » cliqué : le bouton reste grisé jusqu'à la revanche.
	bool bRematchRequested = false;

	FString ShownResult;
	FString ShownOwnCrowns;
	FString ShownEnemyCrowns;
	FString ShownStatus;
	EArenaTeam ShownTeam = EArenaTeam::None;
};

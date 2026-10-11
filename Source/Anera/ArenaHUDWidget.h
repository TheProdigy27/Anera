#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaTypes.h"
#include "ArenaHUDWidget.generated.h"

class AArenaGameState;
class AArenaPlayerController;
class AArenaPlayerState;
class UArenaCardWidget;
class UArenaEndScreenWidget;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;

// Interface de la partie. La mise en page est dans le Widget Blueprint WBP_HUD (enfant de cette classe) :
// - main de 4 cartes et aperçu de la carte suivante, barre d'élixir segmentée ;
// - chrono et couronnes des deux camps ;
// - alertes au centre de l'écran : « Élixir ×2 ! » et « Prolongations ! » ;
// - messages : attente d'un adversaire, adversaire déconnecté, second appui sur Échap pour quitter ;
// - écran de fin de partie (UArenaEndScreenWidget) : résultat, couronnes, Rejouer.
//
// Créée par AArenaHUD sur la machine de chaque joueur. Elle lit à chaque image l'état répliqué (GameState,
// PlayerState) et le PlayerController local, sans rien modifier de la partie. Un widget n'est mis à jour que
// si sa valeur change.
UCLASS(Abstract)
class ANERA_API UArenaHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Le point (en pixels de la vue du jeu, comme APlayerController::GetMousePosition) est-il sur le cadre de la main ?
	bool IsOverHand(const FVector2D& ViewportPixel) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Cartes de la main, dans l'ordre des touches 1 à 4 (WBP_Carte).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UArenaCardWidget> Card1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UArenaCardWidget> Card2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UArenaCardWidget> Card3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UArenaCardWidget> Card4;

	// Aperçu de la carte qui entrera dans la main à la prochaine carte jouée.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UArenaCardWidget> NextCard;

	// Cartes et élixir : masqués tant que le joueur n'a pas de camp.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> HandPanel;

	// Barre d'élixir : le C++ y crée un segment par point d'élixir (10 par défaut).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> ElixirBar;

	// Élixir disponible, en points entiers.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ElixirText;

	// « ×2 » à côté de la barre, tant que l'élixir se recharge deux fois plus vite.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> DoubleElixirBadge;

	// « Temps restant », « Prolongations »…
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TimerText;

	// Couronnes du joueur et de son adversaire, à la couleur de leur camp.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OwnCrownsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyCrownsText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnCrownsLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnemyCrownsLabel;

	// Alerte au centre de l'écran, affichée AlertDuration secondes puis effacée en fondu.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AlertText;

	// Messages d'état, dans un cadre (MessagePanel) masqué quand il n'y a rien à dire.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> MessagePanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	// Rappel des commandes.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HelpText;

	// Écran de fin de partie (WBP_FinPartie), par-dessus le reste.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UArenaEndScreenWidget> EndScreen;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TeamJColor = FLinearColor(0.35f, 0.65f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TeamAColor = FLinearColor(1.f, 0.45f, 0.35f);

	// Partie pleine d'un segment d'élixir.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor ElixirColor = FLinearColor(0.8f, 0.3f, 1.f);

	// Fond d'un segment d'élixir vide.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor ElixirEmptyColor = FLinearColor(0.06f, 0.03f, 0.09f, 0.9f);

	// Écart entre deux segments, en pixels de l'interface.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD", meta = (ClampMin = "0"))
	float ElixirSegmentGap = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TimerColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor OvertimeColor = FLinearColor(1.f, 0.8f, 0.2f);

	// Couleur du chrono dans ses dernières secondes.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD")
	FLinearColor TimerWarningColor = FLinearColor(1.f, 0.3f, 0.25f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD", meta = (ClampMin = "0", Units = "s"))
	float TimerWarningTime = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD", meta = (ClampMin = "0.5", Units = "s"))
	float AlertDuration = 3.f;

	// Durée du fondu à la fin de l'alerte, comprise dans AlertDuration.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|HUD", meta = (ClampMin = "0", Units = "s"))
	float AlertFadeTime = 0.6f;

private:
	void UpdateHand(const AArenaPlayerController* Controller, const AArenaPlayerState* Player);
	void UpdateElixir(const AArenaPlayerController* Controller, const AArenaPlayerState* Player, const AArenaGameState* GameState, float DeltaTime);
	void UpdateMatch(const AArenaGameState* GameState, EArenaTeam OwnTeam);
	void UpdateAlert(const AArenaGameState* GameState, float DeltaTime);
	void UpdateMessages(const AArenaPlayerController* Controller, const AArenaGameState* GameState);

	// Recrée les segments de la barre d'élixir.
	void BuildElixirSegments(int32 Count);

	void ShowAlert(const FText& Text, const FLinearColor& Color);

	FLinearColor GetTeamColor(EArenaTeam Team) const;

	// Change le texte (et la couleur) d'un widget seulement s'ils diffèrent de ce qui est affiché.
	static void SetTextIfChanged(UTextBlock* TextBlock, const FString& Text, FString& Shown);

	TArray<TObjectPtr<UArenaCardWidget>> HandCards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> ElixirSegments;

	// Élixir affiché : suit la valeur reçue du serveur (10 fois par seconde) en la lissant quand elle monte.
	float DisplayedElixir = -1.f;

	// Valeurs déjà affichées.
	FString ShownElixir;
	FString ShownPhase;
	FString ShownTimer;
	FString ShownOwnCrowns;
	FString ShownEnemyCrowns;
	FString ShownMessage;
	FString ShownHelp;
	FLinearColor ShownTimerColor = FLinearColor::Transparent;
	EArenaTeam ShownTeam = EArenaTeam::None;
	bool bShownDoubleElixir = true;

	// Dernier état vu, pour reconnaître le passage à l'élixir ×2 et aux prolongations.
	bool bObservedMatch = false;
	EArenaMatchPhase LastPhase = EArenaMatchPhase::Waiting;
	bool bLastDoubleElixir = false;

	float AlertTimeLeft = 0.f;

	// Adresses de cette machine sur le réseau local, affichées à l'hôte qui attend un adversaire (lues une fois).
	FString LocalAddresses;
};

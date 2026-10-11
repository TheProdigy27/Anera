#include "ArenaEndScreenWidget.h"
#include "ArenaGameInstance.h"
#include "ArenaGameState.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UArenaEndScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RematchButton->OnClicked.AddDynamic(this, &UArenaEndScreenWidget::HandleRematchClicked);
	MenuButton->OnClicked.AddDynamic(this, &UArenaEndScreenWidget::HandleMenuClicked);
	SetVisibility(ESlateVisibility::Collapsed);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UArenaEndScreenWidget::UpdateEndScreen(const AArenaGameState* GameState, const AArenaPlayerState* Player, float DeltaTime)
{
	const bool bEnded = GameState && GameState->GetMatchPhase() == EArenaMatchPhase::Ended;
	TimeSinceEnd = bEnded ? TimeSinceEnd + DeltaTime : 0.f;
	const bool bShow = bEnded && TimeSinceEnd >= ShowDelay;
	if (bShow != bShown)
	{
		bShown = bShow;
		// SelfHitTestInvisible : l'écran lui-même laisse passer les clics, mais son fond (WBP_FinPartie) les arrête.
		SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		// Revanche lancée : le bouton redevient actif pour la prochaine fin de partie.
		bRematchRequested = false;
		RematchButton->SetIsEnabled(true);
	}
	if (!bShow)
	{
		return;
	}
	UpdateResult(GameState, Player ? Player->GetTeam() : EArenaTeam::None);
	UpdateRematchStatus(GameState, Player);
}

void UArenaEndScreenWidget::UpdateResult(const AArenaGameState* GameState, EArenaTeam OwnTeam)
{
	// Sans camp (le temps qu'il arrive du serveur), on affiche le point de vue de J.
	const EArenaTeam Own = OwnTeam == EArenaTeam::None ? EArenaTeam::J : OwnTeam;
	const EArenaTeam Enemy = Own == EArenaTeam::J ? EArenaTeam::A : EArenaTeam::J;
	const EArenaTeam Winner = GameState->GetWinner();
	if (Winner == EArenaTeam::None)
	{
		SetTextIfChanged(ResultText, TEXT("Match nul"), DrawColor, ShownResult);
	}
	else if (Winner == Own)
	{
		SetTextIfChanged(ResultText, TEXT("Victoire !"), VictoryColor, ShownResult);
	}
	else
	{
		SetTextIfChanged(ResultText, TEXT("Défaite"), DefeatColor, ShownResult);
	}

	const FLinearColor OwnColor = Own == EArenaTeam::J ? TeamJColor : TeamAColor;
	const FLinearColor EnemyColor = Own == EArenaTeam::J ? TeamAColor : TeamJColor;
	if (Own != ShownTeam)
	{
		// Les chiffres seront recolorés avec leur prochain texte.
		ShownOwnCrowns.Reset();
		ShownEnemyCrowns.Reset();
		if (OwnCrownsLabel)
		{
			OwnCrownsLabel->SetColorAndOpacity(FSlateColor(OwnColor));
		}
		if (EnemyCrownsLabel)
		{
			EnemyCrownsLabel->SetColorAndOpacity(FSlateColor(EnemyColor));
		}
		ShownTeam = Own;
	}
	SetTextIfChanged(OwnCrownsText, FString::FromInt(GameState->GetCrowns(Own)), OwnColor, ShownOwnCrowns);
	SetTextIfChanged(EnemyCrownsText, FString::FromInt(GameState->GetCrowns(Enemy)), EnemyColor, ShownEnemyCrowns);
}

void UArenaEndScreenWidget::UpdateRematchStatus(const AArenaGameState* GameState, const AArenaPlayerState* Player)
{
	FString Status;
	FLinearColor Color = DrawColor;
	// En solo, la revanche commence au clic : rien à attendre.
	if (GetWorld()->GetNetMode() != NM_Standalone && Player)
	{
		const AArenaPlayerState* Opponent = nullptr;
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			const AArenaPlayerState* OtherArena = Cast<AArenaPlayerState>(Other);
			if (OtherArena && OtherArena != Player && OtherArena->GetTeam() != EArenaTeam::None && OtherArena->GetTeam() != Player->GetTeam())
			{
				Opponent = OtherArena;
				break;
			}
		}
		const bool bRequested = bRematchRequested || Player->WantsRematch();
		if (!Opponent)
		{
			// Seul l'hôte peut rester seul : un client dont l'hôte part revient au menu.
			Status = bRequested
				? TEXT("Réouverture de la partie…")
				: TEXT("Votre adversaire a quitté la partie.\n« Rejouer » rouvre la partie pour un nouvel adversaire.");
		}
		else if (bRequested)
		{
			Status = TEXT("En attente de la réponse de votre adversaire…");
		}
		else if (Opponent->WantsRematch())
		{
			Status = TEXT("Votre adversaire veut une revanche !");
			Color = VictoryColor;
		}
	}
	if (Status != ShownStatus)
	{
		StatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetTextIfChanged(StatusText, Status, Color, ShownStatus);
}

void UArenaEndScreenWidget::HandleRematchClicked()
{
	if (AArenaPlayerController* Controller = GetOwningPlayer<AArenaPlayerController>())
	{
		Controller->RequestRematch();
		bRematchRequested = true;
		RematchButton->SetIsEnabled(false);
	}
}

void UArenaEndScreenWidget::HandleMenuClicked()
{
	if (UArenaGameInstance* GameInstance = GetGameInstance<UArenaGameInstance>())
	{
		GameInstance->ReturnToMenu();
	}
}

void UArenaEndScreenWidget::SetTextIfChanged(UTextBlock* TextBlock, const FString& Text, const FLinearColor& Color, FString& Shown)
{
	if (Text != Shown)
	{
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		Shown = Text;
	}
}

#include "ArenaHUDWidget.h"
#include "ArenaCardData.h"
#include "ArenaCardWidget.h"
#include "ArenaGameInstance.h"
#include "ArenaGameState.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

namespace
{
	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::CeilToInt(FMath::Max(0.f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}
}

void UArenaHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	HandCards = { Card1, Card2, Card3, Card4 };
	for (int32 Index = 0; Index < HandCards.Num(); ++Index)
	{
		HandCards[Index]->SetHandIndex(Index);
	}
	NextCard->SetHandIndex(INDEX_NONE);

	// Seules les cartes de la main reçoivent les clics : partout ailleurs, ils vont au jeu (pose des cartes).
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HandPanel->SetVisibility(ESlateVisibility::Hidden);
	AlertText->SetVisibility(ESlateVisibility::Collapsed);
	MessagePanel->SetVisibility(ESlateVisibility::Collapsed);
	DoubleElixirBadge->SetVisibility(ESlateVisibility::Collapsed);
	bShownDoubleElixir = false;
}

void UArenaHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AArenaPlayerController* Controller = GetOwningPlayer<AArenaPlayerController>();
	const AArenaPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaPlayerState>() : nullptr;
	const AArenaGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;

	UpdateHand(Controller, Player);
	UpdateElixir(Controller, Player, GameState, InDeltaTime);
	// Chez un client, le GameState arrive du serveur peu après l'entrée dans l'arène.
	if (GameState)
	{
		UpdateMatch(GameState, Player ? Player->GetTeam() : EArenaTeam::None);
		UpdateAlert(GameState, InDeltaTime);
		UpdateMessages(Controller, GameState);
	}
}

void UArenaHUDWidget::UpdateHand(const AArenaPlayerController* Controller, const AArenaPlayerState* Player)
{
	// La main n'est distribuée qu'au début de la partie, et arrive du serveur chez un client : rien à montrer avant.
	const TArray<UArenaCardData*> Hand = Player ? Player->GetHand() : TArray<UArenaCardData*>();
	const bool bHasHand = Controller && Player && Player->GetTeam() != EArenaTeam::None && !Hand.IsEmpty();
	const ESlateVisibility PanelVisibility = bHasHand ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
	if (HandPanel->GetVisibility() != PanelVisibility)
	{
		HandPanel->SetVisibility(PanelVisibility);
	}
	if (!bHasHand)
	{
		return;
	}

	// Élixir moins le coût des cartes envoyées dont la réponse n'est pas arrivée.
	const float Elixir = Controller->GetAvailableElixir();
	for (int32 Index = 0; Index < HandCards.Num(); ++Index)
	{
		UArenaCardData* Card = Hand.IsValidIndex(Index) ? Hand[Index] : nullptr;
		UArenaCardWidget* Widget = HandCards[Index];
		Widget->SetCard(Card);
		Widget->SetState(Controller->GetSelectedHandSlot() == Index, Card && Elixir >= Card->ElixirCost, Controller->IsCardPending(Card));
	}
	NextCard->SetCard(Player->GetNextCard());
}

void UArenaHUDWidget::UpdateElixir(const AArenaPlayerController* Controller, const AArenaPlayerState* Player, const AArenaGameState* GameState, float DeltaTime)
{
	if (!Controller || !Player)
	{
		return;
	}
	const int32 Max = FMath::Max(1, FMath::RoundToInt(Player->GetMaxElixir()));
	if (ElixirSegments.Num() != Max)
	{
		BuildElixirSegments(Max);
	}

	// Chez un client, l'élixir arrive du serveur 10 fois par seconde : la barre monterait par petits sauts.
	// La montée est donc lissée ; une dépense s'affiche tout de suite.
	const float Elixir = Controller->GetAvailableElixir();
	DisplayedElixir = DisplayedElixir < 0.f || Elixir < DisplayedElixir ? Elixir : FMath::FInterpTo(DisplayedElixir, Elixir, DeltaTime, 10.f);
	for (int32 Index = 0; Index < ElixirSegments.Num(); ++Index)
	{
		ElixirSegments[Index]->SetPercent(FMath::Clamp(DisplayedElixir - Index, 0.f, 1.f));
	}
	SetTextIfChanged(ElixirText, FString::FromInt(FMath::FloorToInt(DisplayedElixir + KINDA_SMALL_NUMBER)), ShownElixir);

	const bool bDoubleElixir = GameState && GameState->IsDoubleElixir();
	if (bDoubleElixir != bShownDoubleElixir)
	{
		DoubleElixirBadge->SetVisibility(bDoubleElixir ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		bShownDoubleElixir = bDoubleElixir;
	}
}

void UArenaHUDWidget::BuildElixirSegments(int32 Count)
{
	ElixirBar->ClearChildren();
	ElixirSegments.Reset();

	// Segment arrondi : fond sombre, partie pleine teintée par FillColorAndOpacity.
	FProgressBarStyle Style;
	Style.SetBackgroundImage(FSlateRoundedBoxBrush(ElixirEmptyColor, 4.f));
	Style.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 4.f));
	Style.SetMarqueeImage(FSlateNoResource());

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Un widget créé en jeu appartient à l'arbre de widgets (WidgetTree) de ce HUD.
		UProgressBar* Segment = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Segment->SetWidgetStyle(Style);
		Segment->SetFillColorAndOpacity(ElixirColor);
		Segment->SetBarFillType(EProgressBarFillType::LeftToRight);
		Segment->SetPercent(0.f);
		Segment->SetVisibility(ESlateVisibility::HitTestInvisible);

		UHorizontalBoxSlot* SegmentSlot = ElixirBar->AddChildToHorizontalBox(Segment);
		SegmentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SegmentSlot->SetPadding(FMargin(Index == 0 ? 0.f : ElixirSegmentGap, 0.f, 0.f, 0.f));
		SegmentSlot->SetVerticalAlignment(VAlign_Fill);
		ElixirSegments.Add(Segment);
	}
}

void UArenaHUDWidget::UpdateMatch(const AArenaGameState* GameState, EArenaTeam OwnTeam)
{
	FString Phase;
	FString Timer = FormatTime(GameState->GetRemainingTime());
	FLinearColor Color = TimerColor;
	const bool bLastSeconds = GameState->GetRemainingTime() <= TimerWarningTime;
	switch (GameState->GetMatchPhase())
	{
	case EArenaMatchPhase::Waiting:
		Phase = TEXT("En attente");
		Timer = TEXT("–:––");
		break;
	case EArenaMatchPhase::InProgress:
		Phase = TEXT("Temps restant");
		Color = bLastSeconds ? TimerWarningColor : TimerColor;
		break;
	case EArenaMatchPhase::Overtime:
		Phase = TEXT("Prolongations");
		Color = bLastSeconds ? TimerWarningColor : OvertimeColor;
		break;
	case EArenaMatchPhase::Ended:
		Phase = TEXT("Partie terminée");
		Timer = FormatTime(0.f);
		break;
	}
	SetTextIfChanged(PhaseText, Phase, ShownPhase);
	SetTextIfChanged(TimerText, Timer, ShownTimer);
	if (Color != ShownTimerColor)
	{
		TimerText->SetColorAndOpacity(FSlateColor(Color));
		ShownTimerColor = Color;
	}

	// Sans camp (le temps qu'il arrive du serveur), on affiche le point de vue de J.
	const EArenaTeam Own = OwnTeam == EArenaTeam::None ? EArenaTeam::J : OwnTeam;
	const EArenaTeam Enemy = Own == EArenaTeam::J ? EArenaTeam::A : EArenaTeam::J;
	SetTextIfChanged(OwnCrownsText, FString::FromInt(GameState->GetCrowns(Own)), ShownOwnCrowns);
	SetTextIfChanged(EnemyCrownsText, FString::FromInt(GameState->GetCrowns(Enemy)), ShownEnemyCrowns);
	if (Own != ShownTeam)
	{
		const FSlateColor OwnColor(GetTeamColor(Own));
		const FSlateColor EnemyColor(GetTeamColor(Enemy));
		OwnCrownsText->SetColorAndOpacity(OwnColor);
		EnemyCrownsText->SetColorAndOpacity(EnemyColor);
		if (OwnCrownsLabel)
		{
			OwnCrownsLabel->SetColorAndOpacity(OwnColor);
		}
		if (EnemyCrownsLabel)
		{
			EnemyCrownsLabel->SetColorAndOpacity(EnemyColor);
		}
		ShownTeam = Own;
	}
}

void UArenaHUDWidget::UpdateAlert(const AArenaGameState* GameState, float DeltaTime)
{
	// Alerte au passage dans une nouvelle situation, pas à l'arrivée dans une partie déjà dans cet état.
	const EArenaMatchPhase Phase = GameState->GetMatchPhase();
	const bool bDoubleElixir = GameState->IsDoubleElixir();
	if (bObservedMatch)
	{
		if (Phase == EArenaMatchPhase::Overtime && LastPhase != EArenaMatchPhase::Overtime)
		{
			ShowAlert(FText::FromString(TEXT("Prolongations !\nLa prochaine couronne gagne")), OvertimeColor);
		}
		else if (Phase == EArenaMatchPhase::InProgress && bDoubleElixir && !bLastDoubleElixir)
		{
			ShowAlert(FText::FromString(TEXT("Élixir ×2 !")), ElixirColor);
		}
	}
	bObservedMatch = true;
	LastPhase = Phase;
	bLastDoubleElixir = bDoubleElixir;

	if (AlertTimeLeft <= 0.f)
	{
		return;
	}
	AlertTimeLeft -= DeltaTime;
	if (AlertTimeLeft <= 0.f)
	{
		AlertText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	AlertText->SetRenderOpacity(AlertFadeTime > 0.f ? FMath::Clamp(AlertTimeLeft / AlertFadeTime, 0.f, 1.f) : 1.f);
}

void UArenaHUDWidget::ShowAlert(const FText& Text, const FLinearColor& Color)
{
	AlertText->SetText(Text);
	AlertText->SetColorAndOpacity(FSlateColor(Color));
	AlertText->SetRenderOpacity(1.f);
	AlertText->SetVisibility(ESlateVisibility::HitTestInvisible);
	AlertTimeLeft = AlertDuration;
}

void UArenaHUDWidget::UpdateMessages(const AArenaPlayerController* Controller, const AArenaGameState* GameState)
{
	TArray<FString> Lines;
	const ENetMode NetMode = GetWorld()->GetNetMode();

	if (GameState->GetMatchPhase() == EArenaMatchPhase::Waiting)
	{
		Lines.Add(TEXT("En attente d'un adversaire…"));
		// Hôte d'une partie annoncée sur Steam : l'adversaire la trouve par Steam, sans adresse ni port.
		const UArenaSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
		if (NetMode == NM_ListenServer && Sessions && Sessions->IsUsingSteam() && Sessions->IsHostingSession())
		{
			Lines.Add(TEXT("Partie ouverte sur Steam : votre adversaire la choisit dans la liste du menu, ou passe par Steam "
				"(invitation, ou « Rejoindre la partie » dans la liste d'amis). Aucun port à ouvrir."));
		}
		else if (NetMode == NM_ListenServer)
		{
			// Ce qu'il faut à l'adversaire pour rejoindre, et à l'hôte pour régler sa box.
			const int32 Port = UArenaGameInstance::GetListenPort(GetWorld());
			if (LocalAddresses.IsEmpty())
			{
				// Lues une fois : interroger les cartes réseau à chaque image coûterait pour rien.
				LocalAddresses = FString::Join(UArenaGameInstance::GetLocalAddresses(), TEXT(", "));
				LocalAddresses = LocalAddresses.IsEmpty() ? TEXT("inconnue") : LocalAddresses;
			}
			Lines.Add(FString::Printf(TEXT("Partie ouverte sur le port %d (UDP). Adresse sur le réseau local : %s"), Port, *LocalAddresses));
			Lines.Add(FString::Printf(TEXT("Même réseau : l'adversaire entre cette adresse. Par Internet : votre adresse IP publique, "
				"et la box doit rediriger le port %d (UDP) vers l'adresse locale ci-dessus."), Port));
		}
	}

	// En réseau, un camp sans joueur : l'adversaire s'est déconnecté et peut revenir.
	if (NetMode != NM_Standalone && GameState->IsMatchRunning())
	{
		int32 PlayersInTeams = 0;
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			const AArenaPlayerState* OtherArena = Cast<AArenaPlayerState>(Other);
			PlayersInTeams += OtherArena && OtherArena->GetTeam() != EArenaTeam::None ? 1 : 0;
		}
		if (PlayersInTeams < 2)
		{
			Lines.Add(TEXT("Adversaire déconnecté : il peut revenir en rejoignant la partie."));
		}
	}
	if (Controller && Controller->IsQuitPending())
	{
		Lines.Add(TEXT("Appuyez encore sur Échap pour quitter la partie."));
	}

	const FString Message = FString::Join(Lines, TEXT("\n"));
	if (Message != ShownMessage)
	{
		MessageText->SetText(FText::FromString(Message));
		MessagePanel->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		ShownMessage = Message;
	}

	SetTextIfChanged(HelpText, GameState->IsMatchRunning()
		? TEXT("Touches 1 à 4 (ou clic sur une carte) : choisir · clic gauche sur le terrain : poser · clic droit : annuler\n"
			"Échap deux fois : quitter la partie")
		: TEXT("Échap : retour au menu"), ShownHelp);
}

FLinearColor UArenaHUDWidget::GetTeamColor(EArenaTeam Team) const
{
	switch (Team)
	{
	case EArenaTeam::J: return TeamJColor;
	case EArenaTeam::A: return TeamAColor;
	default: return FLinearColor::White;
	}
}

void UArenaHUDWidget::SetTextIfChanged(UTextBlock* TextBlock, const FString& Text, FString& Shown)
{
	if (Text != Shown)
	{
		TextBlock->SetText(FText::FromString(Text));
		Shown = Text;
	}
}

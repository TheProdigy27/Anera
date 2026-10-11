#include "ArenaHUD.h"
#include "Anera.h"
#include "ArenaBuilding.h"
#include "ArenaCardData.h"
#include "ArenaGameState.h"
#include "ArenaHUDWidget.h"
#include "ArenaHealthComponent.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "ArenaUnit.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<bool> CVarHUDDebug(
		TEXT("Arene.HUDDebug"),
		false,
		TEXT("1 : affiche le panneau de débogage du HUD (phase, chrono, élixir, main, PV des bâtiments) en plus de l'interface."));

	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::CeilToInt(FMath::Max(0.f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	const TCHAR* StateName(EArenaBuildingState State)
	{
		switch (State)
		{
		case EArenaBuildingState::Intact: return TEXT("intact");
		case EArenaBuildingState::Damaged: return TEXT("endommagé");
		default: return TEXT("ruine");
		}
	}
}

void AArenaHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* Controller = GetOwningPlayerController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	if (!HUDWidgetClass)
	{
		UE_LOG(LogArene, Warning, TEXT("%s : HUDWidgetClass n'est pas reglee, seul le panneau de debogage est affiche."), *GetClass()->GetName());
		return;
	}
	// CreateWidget fabrique le widget pour ce joueur ; AddToViewport l'affiche par-dessus la vue 3D.
	HUDWidget = CreateWidget<UArenaHUDWidget>(Controller, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
	}
}

void AArenaHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	// Texte un peu plus grand sur les grands écrans (référence : 720 pixels de haut).
	const float Scale = FMath::Max(1.f, Canvas->ClipY / 720.f);
	DrawWorldLabels(Scale);
	if (!HUDWidget || CVarHUDDebug.GetValueOnGameThread())
	{
		DrawDebugPanel(Scale);
	}
	DrawEndBanner(Scale);
}

void AArenaHUD::DrawDebugPanel(float Scale)
{
	const AArenaGameState* GameState = GetWorld()->GetGameState<AArenaGameState>();
	const AArenaPlayerController* Controller = Cast<AArenaPlayerController>(GetOwningPlayerController());
	const AArenaPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaPlayerState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	// Les lignes sont d'abord rassemblées, pour dessiner dessous un fond sombre à leur taille.
	TArray<TPair<FString, FLinearColor>> Lines;
	auto Line = [&Lines](const FString& Text, const FLinearColor& Color)
	{
		Lines.Emplace(Text, Color);
	};

	// Phase et chrono.
	switch (GameState->GetMatchPhase())
	{
	case EArenaMatchPhase::Waiting:
		Line(TEXT("En attente d'un adversaire"), FLinearColor::White);
		break;
	case EArenaMatchPhase::InProgress:
		Line(FString::Printf(TEXT("Temps : %s"), *FormatTime(GameState->GetRemainingTime())), FLinearColor::White);
		break;
	case EArenaMatchPhase::Overtime:
		Line(FString::Printf(TEXT("PROLONGATIONS : %s"), *FormatTime(GameState->GetRemainingTime())), FLinearColor::Yellow);
		break;
	case EArenaMatchPhase::Ended:
		Line(TEXT("Partie terminée"), FLinearColor::White);
		break;
	}
	if (GameState->IsDoubleElixir())
	{
		Line(TEXT("Élixir ×2"), FLinearColor(0.9f, 0.4f, 1.f));
	}
	Line(FString::Printf(TEXT("Couronnes : J %d - %d A"), GameState->GetCrowns(EArenaTeam::J), GameState->GetCrowns(EArenaTeam::A)), FLinearColor::White);

	// Élixir et cartes du joueur local.
	if (Player && Player->GetTeam() != EArenaTeam::None)
	{
		const bool bTeamJ = Player->GetTeam() == EArenaTeam::J;
		Line(FString::Printf(TEXT("Vous jouez le camp %s"), bTeamJ ? TEXT("J") : TEXT("A")), bTeamJ ? TeamJColor : TeamAColor);

		// Cartes envoyées au serveur comprises : leur coût est déjà déduit, en attendant sa réponse.
		const float Elixir = Controller->GetAvailableElixir();
		Line(FString::Printf(TEXT("Élixir : %.2f / %.0f (serveur : %.2f)"), Elixir, Player->GetMaxElixir(), Player->GetElixir()), FLinearColor(0.9f, 0.4f, 1.f));

		const TArray<UArenaCardData*> Hand = Player->GetHand();
		for (int32 Index = 0; Index < Hand.Num(); ++Index)
		{
			const UArenaCardData* Card = Hand[Index];
			if (!Card)
			{
				continue;
			}
			const bool bSelected = Controller->GetSelectedHandSlot() == Index;
			const bool bPending = Controller->IsCardPending(Card);
			const bool bAffordable = !bPending && Elixir >= Card->ElixirCost;
			const FLinearColor Color = bSelected ? FLinearColor::Yellow : bAffordable ? FLinearColor::White : FLinearColor::Gray;
			Line(FString::Printf(TEXT("%s[%d] %s (%d)%s"), bSelected ? TEXT("> ") : TEXT("  "), Index + 1,
				*Card->DisplayName.ToString(), Card->ElixirCost, bPending ? TEXT(" : envoyée…") : TEXT("")), Color);
		}
		if (const UArenaCardData* Next = Player->GetNextCard())
		{
			Line(FString::Printf(TEXT("    Suivante : %s (%d)"), *Next->DisplayName.ToString(), Next->ElixirCost), FLinearColor::Gray);
		}
	}

	// PV des bâtiments.
	Line(FString(), FLinearColor::White);
	for (TActorIterator<AArenaBuilding> It(GetWorld()); It; ++It)
	{
		const AArenaBuilding* Building = *It;
		const UArenaHealthComponent* Health = Building->GetHealthComponent();
		Line(FString::Printf(TEXT("%s : %.0f / %.0f (%s%s)"), *Building->GetGroup().ToString(), Health->GetHealth(), Health->GetMaxHealth(),
			StateName(Building->GetBuildingState()), Building->IsActive() ? TEXT("") : TEXT(", endormi")),
			Building->GetTeam() == EArenaTeam::J ? TeamJColor : TeamAColor);
	}

	UFont* Font = GEngine->GetMediumFont();
	const float LineHeight = 20.f * Scale;
	const float Padding = 8.f * Scale;
	// Sous le cadre des messages de WBP_HUD, en haut à gauche.
	const float X = 16.f * Scale;
	float Y = Canvas->ClipY * 0.3f;
	float MaxWidth = 0.f;
	for (const TPair<FString, FLinearColor>& Entry : Lines)
	{
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Entry.Key, Width, Height, Font, Scale);
		MaxWidth = FMath::Max(MaxWidth, Width);
	}
	DrawRect(PanelColor, X - Padding, Y - Padding, MaxWidth + 2.f * Padding, Lines.Num() * LineHeight + 2.f * Padding);
	for (const TPair<FString, FLinearColor>& Entry : Lines)
	{
		DrawText(Entry.Key, Entry.Value, X, Y, Font, Scale);
		Y += LineHeight;
	}
}

void AArenaHUD::DrawWorldLabels(float Scale)
{
	UFont* Font = GEngine->GetSmallFont();

	for (TActorIterator<AArenaUnit> It(GetWorld()); It; ++It)
	{
		const AArenaUnit* Unit = *It;
		const FVector Screen = Project(Unit->GetVisualLocation() + FVector(0.f, 0.f, 90.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const FString Name = Unit->GetCard() ? Unit->GetCard()->DisplayName.ToString() : Unit->GetName();
		DrawCenteredText(FString::Printf(TEXT("%s %.0f"), *Name, Unit->GetHealthComponent()->GetHealth()),
			Unit->GetTeam() == EArenaTeam::J ? TeamJColor : TeamAColor, Screen.X, Screen.Y, Font, Scale);
	}

	for (TActorIterator<AArenaBuilding> It(GetWorld()); It; ++It)
	{
		const AArenaBuilding* Building = *It;
		if (Building->IsDestroyed())
		{
			continue;
		}
		const FVector Screen = Project(Building->GetAimPoint() + FVector(0.f, 0.f, 250.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		DrawCenteredText(FString::Printf(TEXT("%.0f"), Building->GetHealthComponent()->GetHealth()),
			Building->GetTeam() == EArenaTeam::J ? TeamJColor : TeamAColor, Screen.X, Screen.Y, GEngine->GetMediumFont(), Scale);
	}
}

void AArenaHUD::DrawEndBanner(float Scale)
{
	const AArenaGameState* GameState = GetWorld()->GetGameState<AArenaGameState>();
	if (!GameState || GameState->GetMatchPhase() != EArenaMatchPhase::Ended)
	{
		return;
	}
	const APlayerController* Controller = GetOwningPlayerController();
	const AArenaPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaPlayerState>() : nullptr;
	const EArenaTeam Winner = GameState->GetWinner();

	FString Text = TEXT("Match nul");
	if (Winner != EArenaTeam::None)
	{
		Text = Player && Player->GetTeam() == Winner ? TEXT("Victoire !") : TEXT("Défaite");
	}
	const FLinearColor Color = Winner == EArenaTeam::J ? TeamJColor : Winner == EArenaTeam::A ? TeamAColor : FLinearColor::White;

	// Bandeau sombre sur toute la largeur, texte en grand par-dessus.
	UFont* Font = GEngine->GetLargeFont();
	const float TextScale = Scale * 3.f;
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, TextScale);
	const float CenterY = Canvas->ClipY * 0.4f;
	DrawRect(PanelColor, 0.f, CenterY - Height, Canvas->ClipX, Height * 2.f);
	DrawCenteredText(Text, Color, Canvas->ClipX * 0.5f, CenterY, Font, TextScale);
}

void AArenaHUD::DrawCenteredText(const FString& Text, const FLinearColor& Color, float ScreenX, float ScreenY, UFont* Font, float Scale)
{
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Scale);
	const float Left = ScreenX - Width * 0.5f;
	const float Top = ScreenY - Height * 0.5f;
	// Ombre noire décalée d'un pixel : le texte reste lisible sur le décor clair.
	DrawText(Text, FLinearColor::Black, Left + Scale, Top + Scale, Font, Scale);
	DrawText(Text, Color, Left, Top, Font, Scale);
}

#include "ArenaHUD.h"
#include "ArenaBuilding.h"
#include "ArenaCardData.h"
#include "ArenaGameInstance.h"
#include "ArenaGameState.h"
#include "ArenaHealthComponent.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "ArenaUnit.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"

namespace
{
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
	DrawMatchPanel(Scale);
	DrawEndBanner(Scale);
}

void AArenaHUD::DrawMatchPanel(float Scale)
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

	// Hôte d'une partie annoncée sur Steam : l'adversaire la trouve par Steam, sans adresse ni port.
	const UArenaSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UArenaSessionSubsystem>();
	const bool bSteamHost = GetNetMode() == NM_ListenServer && Sessions && Sessions->IsUsingSteam() && Sessions->IsHostingSession();

	// Phase et chrono.
	switch (GameState->GetMatchPhase())
	{
	case EArenaMatchPhase::Waiting:
		Line(TEXT("En attente d'un adversaire…"), FLinearColor::White);
		if (bSteamHost)
		{
			Line(TEXT("Partie ouverte sur Steam : votre adversaire la choisit dans la liste du menu,"), FLinearColor::White);
			Line(TEXT("ou passe par Steam (invitation, ou « Rejoindre la partie » dans la liste d'amis). Aucun port à ouvrir."), FLinearColor::Gray);
		}
		else if (GetNetMode() == NM_ListenServer)
		{
			// Ce qu'il faut à l'adversaire pour rejoindre, et à l'hôte pour régler sa box.
			const int32 Port = UArenaGameInstance::GetListenPort(GetWorld());
			if (LocalAddresses.IsEmpty())
			{
				// Lues une fois : interroger les cartes réseau à chaque image coûterait pour rien.
				LocalAddresses = FString::Join(UArenaGameInstance::GetLocalAddresses(), TEXT(", "));
				LocalAddresses = LocalAddresses.IsEmpty() ? TEXT("inconnue") : LocalAddresses;
			}
			Line(FString::Printf(TEXT("Partie ouverte sur le port %d (UDP)."), Port), FLinearColor::White);
			Line(FString::Printf(TEXT("Adresse sur le réseau local : %s"), *LocalAddresses), FLinearColor::White);
			Line(TEXT("Même réseau : l'adversaire entre cette adresse. Par Internet : votre adresse IP publique,"), FLinearColor::Gray);
			Line(FString::Printf(TEXT("et la box doit rediriger le port %d (UDP) vers l'adresse locale ci-dessus."), Port), FLinearColor::Gray);
		}
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

	// En réseau, un camp sans joueur : l'adversaire s'est déconnecté et peut revenir.
	if (GetNetMode() != NM_Standalone && GameState->IsMatchRunning())
	{
		int32 PlayersInTeams = 0;
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			const AArenaPlayerState* OtherArena = Cast<AArenaPlayerState>(Other);
			PlayersInTeams += OtherArena && OtherArena->GetTeam() != EArenaTeam::None ? 1 : 0;
		}
		if (PlayersInTeams < 2)
		{
			Line(TEXT("Adversaire déconnecté : il peut revenir en rejoignant la partie."), FLinearColor::Yellow);
		}
	}
	if (Controller && Controller->IsQuitPending())
	{
		Line(TEXT("Appuyez encore sur Échap pour quitter la partie."), FLinearColor::Yellow);
	}

	// Élixir et cartes du joueur local.
	if (Player && Player->GetTeam() != EArenaTeam::None)
	{
		const bool bTeamJ = Player->GetTeam() == EArenaTeam::J;
		Line(FString::Printf(TEXT("Vous jouez le camp %s"), bTeamJ ? TEXT("J") : TEXT("A")), bTeamJ ? TeamJColor : TeamAColor);

		const float Elixir = Player->GetElixir();
		const int32 Max = FMath::RoundToInt(Player->GetMaxElixir());
		const int32 Full = FMath::FloorToInt(Elixir);
		const FString Bar = FString::ChrN(Full, TEXT('#')) + FString::ChrN(FMath::Max(0, Max - Full), TEXT('.'));
		Line(FString::Printf(TEXT("Élixir : %.1f / %d  [%s]"), Elixir, Max, *Bar), FLinearColor(0.9f, 0.4f, 1.f));

		const TArray<UArenaCardData*> Hand = Player->GetHand();
		for (int32 Index = 0; Index < Hand.Num(); ++Index)
		{
			const UArenaCardData* Card = Hand[Index];
			if (!Card)
			{
				continue;
			}
			const bool bSelected = Controller->GetSelectedHandSlot() == Index;
			const bool bAffordable = Elixir >= Card->ElixirCost;
			const FLinearColor Color = bSelected ? FLinearColor::Yellow : bAffordable ? FLinearColor::White : FLinearColor::Gray;
			Line(FString::Printf(TEXT("%s[%d] %s (%d)"), bSelected ? TEXT("> ") : TEXT("  "), Index + 1,
				*Card->DisplayName.ToString(), Card->ElixirCost), Color);
		}
		if (const UArenaCardData* Next = Player->GetNextCard())
		{
			Line(FString::Printf(TEXT("    Suivante : %s (%d)"), *Next->DisplayName.ToString(), Next->ElixirCost), FLinearColor::Gray);
		}
		Line(TEXT("Touches 1-4 : choisir, clic gauche : poser, clic droit : annuler"), FLinearColor::Gray);
	}
	Line(GameState->IsMatchRunning() ? TEXT("Échap (deux fois) : quitter la partie") : TEXT("Échap : retour au menu"), FLinearColor::Gray);

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
	const float X = 16.f * Scale;
	float Y = 16.f * Scale;
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

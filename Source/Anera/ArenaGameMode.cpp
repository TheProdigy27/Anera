#include "ArenaGameMode.h"
#include "Anera.h"
#include "ArenaAIOpponent.h"
#include "ArenaBuilding.h"
#include "ArenaCardData.h"
#include "ArenaGameState.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "Engine/World.h"

AArenaGameMode::AArenaGameMode()
{
	// Le Tick du GameMode fait avancer l'élixir et le chrono (sur le serveur).
	PrimaryActorTick.bCanEverTick = true;

	GameStateClass = AArenaGameState::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	// Pas de personnage à diriger : le joueur pose des cartes depuis une caméra fixe.
	DefaultPawnClass = nullptr;

	BuildingClass = AArenaBuilding::StaticClass();
	AIOpponentClass = AArenaAIOpponent::StaticClass();
}

void AArenaGameMode::StartPlay()
{
	Super::StartPlay();
	TryStartMatch();
}

void AArenaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AArenaPlayerState* NewState = NewPlayer->GetPlayerState<AArenaPlayerState>();
	if (!NewState)
	{
		return;
	}

	// Le premier joueur connecté (l'hôte) prend le camp J, le suivant le camp A.
	bool bTeamJTaken = false;
	for (const APlayerState* Other : GameState->PlayerArray)
	{
		const AArenaPlayerState* OtherState = Cast<AArenaPlayerState>(Other);
		if (OtherState && OtherState != NewState && OtherState->GetTeam() == EArenaTeam::J)
		{
			bTeamJTaken = true;
			break;
		}
	}
	NewState->SetTeam(bTeamJTaken ? EArenaTeam::A : EArenaTeam::J);

	TryStartMatch();
}

void AArenaGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!ArenaGameState || !ArenaGameState->IsMatchRunning())
	{
		return;
	}

	const bool bDoubleElixir = ArenaGameState->GetMatchPhase() == EArenaMatchPhase::Overtime
		|| ArenaGameState->GetRemainingTime() <= DoubleElixirTime;
	if (bDoubleElixir != ArenaGameState->IsDoubleElixir())
	{
		ArenaGameState->SetDoubleElixir(bDoubleElixir);
		UE_LOG(LogArene, Log, TEXT("Elixir x%.0f"), bDoubleElixir ? DoubleElixirMultiplier : 1.f);
	}

	const float ElixirGain = DeltaSeconds / ElixirRegenInterval * (bDoubleElixir ? DoubleElixirMultiplier : 1.f);
	for (APlayerState* Player : GameState->PlayerArray)
	{
		if (AArenaPlayerState* ArenaPlayer = Cast<AArenaPlayerState>(Player))
		{
			ArenaPlayer->AddElixir(ElixirGain);
		}
	}
}

void AArenaGameMode::TryStartMatch()
{
	// PostLogin peut arriver avant StartPlay : on attend que le monde ait commencé.
	AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!ArenaGameState || !GetWorld()->HasBegunPlay() || ArenaGameState->GetMatchPhase() != EArenaMatchPhase::Waiting)
	{
		return;
	}

	const int32 RequiredPlayers = GetNetMode() == NM_Standalone ? 1 : 2;
	int32 ReadyPlayers = 0;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayer = Cast<AArenaPlayerState>(Player);
		if (ArenaPlayer && ArenaPlayer->GetTeam() != EArenaTeam::None)
		{
			++ReadyPlayers;
		}
	}
	if (ReadyPlayers >= RequiredPlayers)
	{
		StartMatch();
	}
}

void AArenaGameMode::StartMatch()
{
	AArenaGameState* ArenaGameState = GetArenaGameState();
	for (APlayerState* Player : GameState->PlayerArray)
	{
		if (AArenaPlayerState* ArenaPlayer = Cast<AArenaPlayerState>(Player))
		{
			ArenaPlayer->InitializeElixir(StartElixir, MaxElixir);
			const TArray<UArenaCardData*> Deck = GetDeckFor(ArenaPlayer);
			if (Deck.Num() != 8)
			{
				UE_LOG(LogArene, Warning, TEXT("Deck de %s : %d cartes au lieu de 8 (DefaultDeck du GameMode)."), *ArenaPlayer->GetPlayerName(), Deck.Num());
			}
			ArenaPlayer->InitializeDeck(Deck);
		}
	}

	ArenaGameState->SetDoubleElixir(false);
	ArenaGameState->SetMatchPhase(EArenaMatchPhase::InProgress, MatchDuration);
	UE_LOG(LogArene, Log, TEXT("Debut de la partie (%d joueurs, %.0f s)"), GameState->PlayerArray.Num(), MatchDuration);
}

AArenaGameState* AArenaGameMode::GetArenaGameState() const
{
	return GetGameState<AArenaGameState>();
}

TArray<UArenaCardData*> AArenaGameMode::GetDeckFor(const AArenaPlayerState* Player) const
{
	return ObjectPtrDecay(DefaultDeck);
}

bool AArenaGameMode::TryPlayCard(AArenaPlayerState* Player, UArenaCardData* Card, const FVector& Location)
{
	UE_LOG(LogArene, Warning, TEXT("TryPlayCard(%s, %s, %s) : pose des cartes pas encore implementee (1.2.4)."),
		*GetNameSafe(Player), *GetNameSafe(Card), *Location.ToCompactString());
	return false;
}

#include "ArenaGameMode.h"
#include "Anera.h"
#include "ArenaAIOpponent.h"
#include "ArenaBuilding.h"
#include "ArenaCardData.h"
#include "ArenaCombat.h"
#include "ArenaDeck.h"
#include "ArenaGameInstance.h"
#include "ArenaGameState.h"
#include "ArenaHealthComponent.h"
#include "ArenaHUD.h"
#include "ArenaLevel.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "ArenaProjectile.h"
#include "ArenaUnit.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	// Variable de console : modifiable pendant le jeu, et lue ici au début de la partie.
	TAutoConsoleVariable<bool> CVarDemoMode(
		TEXT("Arene.Demo"),
		false,
		TEXT("Arene.Demo 1 : une IA joue à la place de chaque joueur humain dans les parties qui commencent ensuite ")
		TEXT("(comme bDemoMode du GameMode, sans toucher au Blueprint)."));
}

AArenaGameMode::AArenaGameMode()
{
	// Le Tick du GameMode fait avancer l'élixir et le chrono (sur le serveur).
	PrimaryActorTick.bCanEverTick = true;

	GameStateClass = AArenaGameState::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	HUDClass = AArenaHUD::StaticClass();
	// Pas de personnage à diriger : le joueur pose des cartes depuis une caméra fixe.
	DefaultPawnClass = nullptr;

	BuildingClass = AArenaBuilding::StaticClass();
	AIOpponentClass = AArenaAIOpponent::StaticClass();
}

void AArenaGameMode::StartPlay()
{
	Super::StartPlay();
	SpawnBuildings();
	TryStartMatch();
}

void AArenaGameMode::SpawnBuildings()
{
	TArray<AActor*> Slots;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("slot"), Slots);

	for (const AActor* Slot : Slots)
	{
		const FString Type = ArenaLevel::GetTagValue(Slot, TEXT("type"));
		if (Type != TEXT("tour") && Type != TEXT("fort"))
		{
			continue;
		}
		const EArenaTeam Team = ArenaLevel::GetTagTeam(Slot);

		// Le groupe (« Tour_Gauche_J »…) est lu sur le SLOT_Unite_* posé au sommet du bâtiment (tag « parent: »).
		// Les noms affichés dans l'Outliner n'existent pas dans un jeu packagé : on ne s'en sert pas.
		const FString TopType = Type == TEXT("tour") ? TEXT("unite_tour") : TEXT("unite_fort");
		const AActor* Top = nullptr;
		for (const AActor* Candidate : Slots)
		{
			if (ArenaLevel::GetTagValue(Candidate, TEXT("type")) == TopType && ArenaLevel::GetTagTeam(Candidate) == Team
				&& (!Top || FVector::Dist2D(Candidate->GetActorLocation(), Slot->GetActorLocation())
					< FVector::Dist2D(Top->GetActorLocation(), Slot->GetActorLocation())))
			{
				Top = Candidate;
			}
		}
		const FString Group = ArenaLevel::GetTagValue(Top, TEXT("parent"));
		if (Group.IsEmpty())
		{
			UE_LOG(LogArene, Warning, TEXT("Slot %s : aucun slot '%s' avec un tag 'parent:' a proximite, batiment ignore."), *Slot->GetName(), *TopType);
			continue;
		}

		const FTransform SpawnTransform(Slot->GetActorRotation(), Slot->GetActorLocation());
		AArenaBuilding* Building = GetWorld()->SpawnActorDeferred<AArenaBuilding>(BuildingClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Building)
		{
			continue;
		}
		Building->InitializeBuilding(FName(*Group), Type == TEXT("fort") ? EArenaBuildingType::Fort : EArenaBuildingType::Tower,
			Team, Top->GetActorLocation());
		Building->FinishSpawning(SpawnTransform);
		Building->OnBuildingDestroyed.AddDynamic(this, &AArenaGameMode::HandleBuildingDestroyed);
		Buildings.Add(Building);
	}
	UE_LOG(LogArene, Log, TEXT("%d batiments places"), Buildings.Num());
}

void AArenaGameMode::HandleBuildingDestroyed(AArenaBuilding* Building)
{
	AArenaGameState* ArenaGameState = GetArenaGameState();
	const EArenaTeam Scorer = ArenaLevel::GetOpponent(Building->GetTeam());
	if (!ArenaGameState || Scorer == EArenaTeam::None)
	{
		return;
	}

	if (!ArenaGameState->IsMatchRunning())
	{
		return;
	}

	if (Building->GetBuildingType() == EArenaBuildingType::Fort)
	{
		// Le fort vaut toutes les couronnes (3 au total) et la victoire immédiate.
		ArenaGameState->SetCrowns(Scorer, 3);
		EndMatch(Scorer);
		return;
	}

	ArenaGameState->AddCrowns(Scorer, 1);
	UE_LOG(LogArene, Log, TEXT("Couronnes : J %d - A %d"), ArenaGameState->GetCrowns(EArenaTeam::J), ArenaGameState->GetCrowns(EArenaTeam::A));

	// Une tour tombée réveille le fort de son camp.
	for (AArenaBuilding* Other : Buildings)
	{
		if (Other && Other->GetTeam() == Building->GetTeam() && Other->GetBuildingType() == EArenaBuildingType::Fort)
		{
			Other->Activate();
		}
	}

	// En prolongations, la première couronne l'emporte.
	if (ArenaGameState->GetMatchPhase() == EArenaMatchPhase::Overtime)
	{
		EndMatch(Scorer);
	}
}

namespace
{
	// Messages renvoyés au joueur refusé, affichés dans son menu.
	const TCHAR* ArenaFullMessage = TEXT("La partie est complète (2 joueurs).");
	const TCHAR* ArenaEndedMessage = TEXT("Cette partie est terminée.");
}

bool AArenaGameMode::IsFull() const
{
	int32 HumanPlayers = 0;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		if (Player && !Player->IsABot())
		{
			++HumanPlayers;
		}
	}
	return HumanPlayers >= 2;
}

const TCHAR* AArenaGameMode::GetJoinRefusal() const
{
	if (IsFull())
	{
		return ArenaFullMessage;
	}
	const AArenaGameState* ArenaGameState = GetArenaGameState();
	if (ArenaGameState && ArenaGameState->GetMatchPhase() == EArenaMatchPhase::Ended)
	{
		return ArenaEndedMessage;
	}
	return nullptr;
}

void AArenaGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	const TCHAR* Refusal = ErrorMessage.IsEmpty() ? GetJoinRefusal() : nullptr;
	if (Refusal)
	{
		ErrorMessage = Refusal;
		UE_LOG(LogArene, Log, TEXT("Connexion de %s refusee : %s"), *Address, Refusal);
	}
}

APlayerController* AArenaGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	if (const TCHAR* Refusal = GetJoinRefusal())
	{
		ErrorMessage = Refusal;
		UE_LOG(LogArene, Log, TEXT("Entree de %s refusee : %s"), *UGameplayStatics::ParseOption(Options, TEXT("Name")), Refusal);
		return nullptr;
	}
	return Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
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

	// Arrivée en cours de partie (retour après une déconnexion) : élixir et cartes tout de suite.
	const AArenaGameState* ArenaGameState = GetArenaGameState();
	if (ArenaGameState && ArenaGameState->IsMatchRunning())
	{
		InitializePlayerForMatch(NewState);
	}

	TryStartMatch();
}

FString AArenaGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options,
	const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	AArenaPlayerState* NewState = NewPlayerController ? NewPlayerController->GetPlayerState<AArenaPlayerState>() : nullptr;
	if (!Error.IsEmpty() || !NewState || !UGameplayStatics::HasOption(Options, TEXT("Deck")))
	{
		return Error;
	}
	// Le deck vient du joueur : il n'est accepté que s'il est jouable (8 cartes différentes, toutes du jeu).
	const TArray<UArenaCardData*> Deck = ArenaDeck::ParseDeckOption(Options);
	if (ArenaDeck::IsValidDeck(Deck))
	{
		NewState->SetChosenDeck(Deck);
		UE_LOG(LogArene, Log, TEXT("Deck de %s recu"), *NewState->GetPlayerName());
	}
	else
	{
		UE_LOG(LogArene, Warning, TEXT("Deck de %s refuse (%s) : deck par defaut."), *NewState->GetPlayerName(),
			*UGameplayStatics::ParseOption(Options, TEXT("Deck")).Left(300));
	}
	return Error;
}

void AArenaGameMode::Logout(AController* Exiting)
{
	const AArenaPlayerState* State = Exiting ? Exiting->GetPlayerState<AArenaPlayerState>() : nullptr;
	const AArenaGameState* ArenaGameState = GetArenaGameState();
	// Quand l'arène elle-même se ferme (l'hôte quitte), tous les joueurs sortent : rien à garder.
	const bool bArenaClosing = GetWorld()->bIsTearingDown;
	if (State && !State->IsABot() && State->GetTeam() != EArenaTeam::None && ArenaGameState && ArenaGameState->IsMatchRunning() && !bArenaClosing)
	{
		// La partie continue sans lui (ses bâtiments se défendent seuls) : qui rejoindra prendra sa place et ses cartes.
		FArenaAbsentPlayer& Absent = AbsentPlayers.FindOrAdd(State->GetTeam());
		Absent.Elixir = State->GetElixir();
		Absent.Cards = State->GetDeckInOrder();
		UE_LOG(LogArene, Log, TEXT("%s quitte la partie : le camp %s attend son retour"), *State->GetPlayerName(),
			State->GetTeam() == EArenaTeam::J ? TEXT("J") : TEXT("A"));
	}
	Super::Logout(Exiting);

	// Sur l'écran de fin, l'hôte qui attendait la réponse de l'adversaire parti n'attend plus que lui-même. Son
	// PlayerState n'est retiré de la liste des joueurs qu'après Logout : on vérifie à l'image suivante.
	if (ArenaGameState && ArenaGameState->GetMatchPhase() == EArenaMatchPhase::Ended && !bArenaClosing)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AArenaGameMode::TryRestartMatch);
	}
}

void AArenaGameMode::RequestRematch(AArenaPlayerState* Player)
{
	const AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!Player || Player->IsABot() || !ArenaGameState || ArenaGameState->GetMatchPhase() != EArenaMatchPhase::Ended || Player->WantsRematch())
	{
		return;
	}
	Player->SetWantsRematch(true);
	UE_LOG(LogArene, Log, TEXT("%s demande une revanche"), *Player->GetPlayerName());
	TryRestartMatch();
}

void AArenaGameMode::TryRestartMatch()
{
	const AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!ArenaGameState || ArenaGameState->GetMatchPhase() != EArenaMatchPhase::Ended)
	{
		return;
	}
	bool bAnyRequest = false;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayer = Cast<AArenaPlayerState>(Player);
		if (!ArenaPlayer || ArenaPlayer->IsABot() || ArenaPlayer->GetTeam() == EArenaTeam::None)
		{
			continue;
		}
		if (!ArenaPlayer->WantsRematch())
		{
			return;
		}
		bAnyRequest = true;
	}
	if (bAnyRequest)
	{
		RestartMatch();
	}
}

void AArenaGameMode::RestartMatch()
{
	UE_LOG(LogArene, Log, TEXT("Revanche : remise a neuf de l'arene"));

	// Sorts lancés juste avant la fin (minuteries de ResolveSpell) : le GameMode n'a pas d'autre minuterie.
	GetWorldTimerManager().ClearAllTimersForObject(this);

	// Les destructions sont répliquées : unités et projectiles disparaissent aussi chez les clients.
	TArray<AActor*> Leftovers;
	for (TActorIterator<AArenaUnit> It(GetWorld()); It; ++It)
	{
		Leftovers.Add(*It);
	}
	for (TActorIterator<AArenaProjectile> It(GetWorld()); It; ++It)
	{
		Leftovers.Add(*It);
	}
	// Bâtiments neufs : chacun affiche les acteurs importés de son état Intact, chez tous les joueurs.
	for (AArenaBuilding* Building : Buildings)
	{
		Leftovers.Add(Building);
	}
	for (AActor* Actor : Leftovers)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Buildings.Reset();
	SpawnBuildings();

	for (APlayerState* Player : GameState->PlayerArray)
	{
		if (AArenaPlayerState* ArenaPlayer = Cast<AArenaPlayerState>(Player))
		{
			ArenaPlayer->ResetForNewMatch();
		}
	}
	AbsentPlayers.Reset();
	GetArenaGameState()->ResetMatch();
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
	for (TPair<EArenaTeam, FArenaAbsentPlayer>& Absent : AbsentPlayers)
	{
		Absent.Value.Elixir = FMath::Min(Absent.Value.Elixir + ElixirGain, MaxElixir);
	}

	if (ArenaGameState->GetRemainingTime() <= 0.f)
	{
		if (ArenaGameState->GetMatchPhase() == EArenaMatchPhase::InProgress)
		{
			HandleRegularTimeOver();
		}
		else
		{
			EndMatch(GetTieBreakWinner());
		}
	}
}

void AArenaGameMode::HandleRegularTimeOver()
{
	const AArenaGameState* ArenaGameState = GetArenaGameState();
	const int32 CrownsJ = ArenaGameState->GetCrowns(EArenaTeam::J);
	const int32 CrownsA = ArenaGameState->GetCrowns(EArenaTeam::A);
	if (CrownsJ != CrownsA)
	{
		EndMatch(CrownsJ > CrownsA ? EArenaTeam::J : EArenaTeam::A);
	}
	else if (OvertimeDuration > 0.f)
	{
		GetArenaGameState()->SetMatchPhase(EArenaMatchPhase::Overtime, OvertimeDuration);
		UE_LOG(LogArene, Log, TEXT("Egalite %d - %d : prolongations (%.0f s)"), CrownsJ, CrownsA, OvertimeDuration);
	}
	else
	{
		EndMatch(GetTieBreakWinner());
	}
}

void AArenaGameMode::EndMatch(EArenaTeam Winner)
{
	AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!ArenaGameState || !ArenaGameState->IsMatchRunning())
	{
		return;
	}
	// Plus de chrono ni d'élixir : unités et bâtiments s'arrêtent d'eux-mêmes en voyant la phase Fin.
	ArenaGameState->SetMatchPhase(EArenaMatchPhase::Ended, 0.f);
	ArenaGameState->SetDoubleElixir(false);
	ArenaGameState->SetWinner(Winner);
	UE_LOG(LogArene, Log, TEXT("Fin de la partie : %s (couronnes J %d - A %d)"),
		Winner == EArenaTeam::J ? TEXT("victoire de J") : Winner == EArenaTeam::A ? TEXT("victoire de A") : TEXT("match nul"),
		ArenaGameState->GetCrowns(EArenaTeam::J), ArenaGameState->GetCrowns(EArenaTeam::A));
}

void AArenaGameMode::ForceEndMatch(EArenaTeam Winner, int32 CrownsJ, int32 CrownsA)
{
	AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!ArenaGameState || !ArenaGameState->IsMatchRunning())
	{
		UE_LOG(LogArene, Warning, TEXT("Arene.FinPartie : aucune partie en cours."));
		return;
	}
	ArenaGameState->SetCrowns(EArenaTeam::J, FMath::Clamp(CrownsJ, 0, 3));
	ArenaGameState->SetCrowns(EArenaTeam::A, FMath::Clamp(CrownsA, 0, 3));
	EndMatch(Winner);
}

namespace
{
	// Commande de test : « Arene.FinPartie [J|A|N] [couronnes J] [couronnes A] » termine la partie en cours, sans attendre
	// le chrono : victoire de J (par défaut), de A, ou match nul (N). Sur le serveur (en PIE, la console vise l'hôte).
	void EndMatchFromConsole(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		AArenaGameMode* GameMode = ConsoleWorld ? ConsoleWorld->GetAuthGameMode<AArenaGameMode>() : nullptr;
		if (!GameMode)
		{
			UE_LOG(LogArene, Warning, TEXT("Arene.FinPartie : seulement sur le serveur, dans l'arene."));
			return;
		}
		const FString WinnerArg = Args.IsValidIndex(0) ? Args[0].ToUpper() : TEXT("J");
		const EArenaTeam Winner = WinnerArg == TEXT("A") ? EArenaTeam::A : WinnerArg == TEXT("N") ? EArenaTeam::None : EArenaTeam::J;
		const int32 CrownsJ = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : (Winner == EArenaTeam::J ? 1 : 0);
		const int32 CrownsA = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : (Winner == EArenaTeam::A ? 1 : 0);
		UE_LOG(LogArene, Log, TEXT("Arene.FinPartie %s %d %d"), *WinnerArg, CrownsJ, CrownsA);
		GameMode->ForceEndMatch(Winner, CrownsJ, CrownsA);
	}

	FAutoConsoleCommandWithWorldAndArgs EndMatchCommand(
		TEXT("Arene.FinPartie"),
		TEXT("Arene.FinPartie [J|A|N] [couronnes J] [couronnes A] : termine la partie en cours (victoire de J, de A, ou match nul) (test)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&EndMatchFromConsole));
}

EArenaTeam AArenaGameMode::GetTieBreakWinner() const
{
	float WorstJ = 1.f;
	float WorstA = 1.f;
	for (const AArenaBuilding* Building : Buildings)
	{
		if (!Building || Building->IsDestroyed())
		{
			continue;
		}
		const float Fraction = Building->GetHealthComponent()->GetHealthFraction();
		float& Worst = Building->GetTeam() == EArenaTeam::J ? WorstJ : WorstA;
		Worst = FMath::Min(Worst, Fraction);
	}
	UE_LOG(LogArene, Log, TEXT("Departage : batiment le plus abime J %.0f %% - A %.0f %%"), WorstJ * 100.f, WorstA * 100.f);
	if (FMath::IsNearlyEqual(WorstJ, WorstA, 0.0001f))
	{
		return EArenaTeam::None;
	}
	return WorstJ > WorstA ? EArenaTeam::J : EArenaTeam::A;
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

void AArenaGameMode::SpawnAIOpponents()
{
	// Copie de la liste : l'IA du solo y ajoute son propre PlayerState.
	const TArray<TObjectPtr<APlayerState>> HumanPlayers = GameState->PlayerArray;

	if (GetNetMode() == NM_Standalone)
	{
		SpawnAIOpponent(EArenaTeam::A, nullptr);
	}

	if (bDemoMode || CVarDemoMode.GetValueOnGameThread())
	{
		for (APlayerState* Player : HumanPlayers)
		{
			AArenaPlayerState* ArenaPlayer = Cast<AArenaPlayerState>(Player);
			if (ArenaPlayer && ArenaPlayer->GetTeam() != EArenaTeam::None)
			{
				SpawnAIOpponent(ArenaPlayer->GetTeam(), ArenaPlayer);
			}
		}
	}
}

AArenaAIOpponent* AArenaGameMode::SpawnAIOpponent(EArenaTeam Team, AArenaPlayerState* ExistingState)
{
	// Apparition différée : camp et PlayerState sont réglés avant son BeginPlay.
	AArenaAIOpponent* Opponent = GetWorld()->SpawnActorDeferred<AArenaAIOpponent>(AIOpponentClass, FTransform::Identity);
	if (!Opponent)
	{
		return nullptr;
	}
	Opponent->SetTeam(Team);
	Opponent->SetControlledState(ExistingState);
	Opponent->FinishSpawning(FTransform::Identity);
	AIOpponents.Add(Opponent);
	UE_LOG(LogArene, Log, TEXT("IA du camp %s%s"), Team == EArenaTeam::J ? TEXT("J") : TEXT("A"), ExistingState ? TEXT(" (demo)") : TEXT(""));
	return Opponent;
}

void AArenaGameMode::StartMatch()
{
	// À la revanche, les IA de la partie précédente sont toujours là.
	if (AIOpponents.IsEmpty())
	{
		SpawnAIOpponents();
	}

	AArenaGameState* ArenaGameState = GetArenaGameState();
	AbsentPlayers.Reset();
	for (APlayerState* Player : GameState->PlayerArray)
	{
		if (AArenaPlayerState* ArenaPlayer = Cast<AArenaPlayerState>(Player))
		{
			InitializePlayerForMatch(ArenaPlayer);
		}
	}

	ArenaGameState->SetDoubleElixir(false);
	ArenaGameState->SetMatchPhase(EArenaMatchPhase::InProgress, MatchDuration);
	UE_LOG(LogArene, Log, TEXT("Debut de la partie (%d joueurs, %.0f s)"), GameState->PlayerArray.Num(), MatchDuration);
}

void AArenaGameMode::InitializePlayerForMatch(AArenaPlayerState* Player)
{
	FArenaAbsentPlayer Absent;
	if (AbsentPlayers.RemoveAndCopyValue(Player->GetTeam(), Absent))
	{
		// Retour dans un camp quitté en cours de partie : mêmes cartes dans le même ordre, élixir accumulé entre-temps.
		Player->InitializeElixir(Absent.Elixir, MaxElixir);
		Player->InitializeDeck(ObjectPtrDecay(Absent.Cards), false);
		UE_LOG(LogArene, Log, TEXT("%s reprend le camp %s (elixir %.1f)"), *Player->GetPlayerName(),
			Player->GetTeam() == EArenaTeam::J ? TEXT("J") : TEXT("A"), Player->GetElixir());
		return;
	}

	Player->InitializeElixir(StartElixir, MaxElixir);
	const TArray<UArenaCardData*> Deck = GetDeckFor(Player);
	if (Deck.Num() != ArenaDeck::DeckSize)
	{
		UE_LOG(LogArene, Warning, TEXT("Deck de %s : %d cartes au lieu de %d (DefaultDeck de BP_ArenaGameInstance)."), *Player->GetPlayerName(),
			Deck.Num(), ArenaDeck::DeckSize);
	}
	Player->InitializeDeck(Deck, true);
}

AArenaGameState* AArenaGameMode::GetArenaGameState() const
{
	return GetGameState<AArenaGameState>();
}

TArray<UArenaCardData*> AArenaGameMode::GetDeckFor(const AArenaPlayerState* Player) const
{
	// Deck choisi par le joueur dans le menu, vérifié à son arrivée.
	if (Player && !Player->GetChosenDeck().IsEmpty())
	{
		return Player->GetChosenDeck();
	}
	// Une IA peut avoir son propre deck (réglé dans son Blueprint).
	const AArenaAIOpponent* Opponent = Player ? Cast<AArenaAIOpponent>(Player->GetOwner()) : nullptr;
	if (Opponent && !Opponent->GetDeck().IsEmpty())
	{
		return Opponent->GetDeck();
	}
	const UArenaGameInstance* GameInstance = GetGameInstance<UArenaGameInstance>();
	return GameInstance ? GameInstance->GetDefaultDeck() : TArray<UArenaCardData*>();
}

bool AArenaGameMode::TryPlayCard(AArenaPlayerState* Player, UArenaCardData* Card, const FVector& Location)
{
	// Le client n'est jamais cru sur parole : tout est revérifié ici.
	auto Reject = [&](const TCHAR* Reason)
	{
		UE_LOG(LogArene, Log, TEXT("Carte %s refusee pour %s : %s"), *GetNameSafe(Card), *GetNameSafe(Player), Reason);
		return false;
	};

	const AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!Player || !Card)
	{
		return Reject(TEXT("joueur ou carte invalide"));
	}
	if (!ArenaGameState || !ArenaGameState->IsMatchRunning())
	{
		return Reject(TEXT("la partie n'est pas en cours"));
	}
	const int32 HandIndex = Player->FindCardInHand(Card);
	if (HandIndex == INDEX_NONE)
	{
		return Reject(TEXT("la carte n'est pas dans la main"));
	}
	if (Player->GetElixir() < Card->ElixirCost)
	{
		return Reject(TEXT("pas assez d'elixir"));
	}
	FVector GroundLocation;
	if (!ArenaLevel::IsValidPlacement(this, Player->GetTeam(), Card, Location, GroundLocation))
	{
		return Reject(TEXT("emplacement hors de la zone de deploiement ou de la NavMesh"));
	}

	Player->SpendElixir(Card->ElixirCost);
	Player->CycleCard(HandIndex);

	if (Card->CardType == EArenaCardType::Unit)
	{
		SpawnUnits(Card, Player->GetTeam(), GroundLocation);
	}
	else
	{
		FTimerHandle SpellTimer;
		GetWorldTimerManager().SetTimer(SpellTimer,
			FTimerDelegate::CreateUObject(this, &AArenaGameMode::ResolveSpell, Card, Player->GetTeam(), GroundLocation),
			FMath::Max(DeployDelay, KINDA_SMALL_NUMBER), false);
	}

	UE_LOG(LogArene, Log, TEXT("%s joue %s en %s (elixir restant %.1f)"),
		*Player->GetPlayerName(), *Card->DisplayName.ToString(), *GroundLocation.ToCompactString(), Player->GetElixir());
	return true;
}

void AArenaGameMode::SpawnUnits(UArenaCardData* Card, EArenaTeam Team, const FVector& Location)
{
	const TSubclassOf<AArenaUnit> UnitClass = Card->UnitClass ? Card->UnitClass : TSubclassOf<AArenaUnit>(AArenaUnit::StaticClass());
	const float SpawnHeight = UnitClass->GetDefaultObject<AArenaUnit>()->GetSpawnHeight(Card->bFlying);
	const FRotator Facing = ArenaLevel::GetAttackDirection(this, Team).Rotation();

	// Plusieurs unités : réparties sur un petit cercle autour du point visé.
	const int32 Count = FMath::Max(1, Card->UnitCount);
	const float FormationRadius = Count > 1 ? 40.f + 10.f * Count : 0.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Offset = FRotator(0.f, Facing.Yaw + 45.f + 360.f * Index / Count, 0.f).RotateVector(FVector(FormationRadius, 0.f, 0.f));
		// Au sol, chaque unité doit tomber sur la NavMesh reliée au terrain ; sinon elle apparaît au point visé.
		FVector Ground = Location + Offset;
		FVector OnNavMesh;
		if (!Card->bFlying)
		{
			const bool bValid = ArenaLevel::ProjectToNavMesh(this, Ground, OnNavMesh) && ArenaLevel::IsConnectedToLanes(this, Team, OnNavMesh);
			Ground = bValid ? OnNavMesh : Location;
		}

		const FTransform SpawnTransform(Facing, Ground + FVector(0.f, 0.f, SpawnHeight));
		// Apparition « différée » : on règle l'unité avant que son BeginPlay ne s'exécute.
		AArenaUnit* Unit = GetWorld()->SpawnActorDeferred<AArenaUnit>(UnitClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Unit)
		{
			Unit->InitializeUnit(Card, Team, DeployDelay);
			Unit->FinishSpawning(SpawnTransform);
		}
	}
}

void AArenaGameMode::ResolveSpell(UArenaCardData* Card, EArenaTeam Team, FVector Location)
{
	const AArenaGameState* ArenaGameState = GetArenaGameState();
	if (!Card || !ArenaGameState || !ArenaGameState->IsMatchRunning())
	{
		return;
	}
	ArenaCombat::ApplyAreaDamage(GetWorld(), Location, Card->SpellRadius, Card->SpellDamage, Team,
		EArenaTargetType::GroundAndAir, Card->BuildingDamageScale, nullptr);
	ArenaCombat::ApplyAreaSlow(GetWorld(), Location, Card->SpellRadius, Team, Card->SlowAmount, Card->EffectDuration);
	ArenaCombat::PlayEffect(GetWorld(), EArenaEffectType::Spell, Location, Card->SpellRadius, Card->EffectDuration, Team, Card);
	UE_LOG(LogArene, Log, TEXT("Sort %s en %s"), *Card->DisplayName.ToString(), *Location.ToCompactString());
}

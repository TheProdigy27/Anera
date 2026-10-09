#include "ArenaGameMode.h"
#include "Anera.h"
#include "ArenaAIOpponent.h"
#include "ArenaBuilding.h"
#include "ArenaCardData.h"
#include "ArenaCombat.h"
#include "ArenaGameState.h"
#include "ArenaLevel.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "ArenaUnit.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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

	if (Building->GetBuildingType() == EArenaBuildingType::Fort)
	{
		// Le fort vaut toutes les couronnes : 3 au total.
		ArenaGameState->SetCrowns(Scorer, 3);
	}
	else
	{
		ArenaGameState->AddCrowns(Scorer, 1);
		// Une tour tombée réveille le fort de son camp.
		for (AArenaBuilding* Other : Buildings)
		{
			if (Other && Other->GetTeam() == Building->GetTeam() && Other->GetBuildingType() == EArenaBuildingType::Fort)
			{
				Other->Activate();
			}
		}
	}
	UE_LOG(LogArene, Log, TEXT("Couronnes : J %d - A %d"), ArenaGameState->GetCrowns(EArenaTeam::J), ArenaGameState->GetCrowns(EArenaTeam::A));
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
		FVector Ground = Location + Offset;
		if (!Card->bFlying && !ArenaLevel::ProjectToNavMesh(this, Ground, Ground))
		{
			Ground = Location;
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
	UE_LOG(LogArene, Log, TEXT("Sort %s en %s"), *Card->DisplayName.ToString(), *Location.ToCompactString());
}

#include "ArenaAIOpponent.h"
#include "ArenaCardData.h"
#include "ArenaCombat.h"
#include "ArenaGameMode.h"
#include "ArenaGameState.h"
#include "ArenaLevel.h"
#include "ArenaPlayerState.h"
#include "ArenaUnit.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AArenaAIOpponent::AArenaAIOpponent()
{
	// Logique serveur uniquement : rien à envoyer aux clients.
	bReplicates = false;
}

void AArenaAIOpponent::SetControlledState(AArenaPlayerState* State)
{
	OpponentState = State;
}

void AArenaAIOpponent::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}

	if (!OpponentState)
	{
		// Un PlayerState comme celui d'un joueur : il s'inscrit tout seul dans le GameState.
		const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
		UClass* StateClass = GameMode && GameMode->PlayerStateClass ? GameMode->PlayerStateClass.Get() : AArenaPlayerState::StaticClass();
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		OpponentState = GetWorld()->SpawnActor<AArenaPlayerState>(StateClass, SpawnParams);
		if (OpponentState)
		{
			OpponentState->SetTeam(Team);
			OpponentState->SetPlayerName(TEXT("IA"));
			OpponentState->SetIsABot(true);
		}
	}

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AArenaAIOpponent::Think, DecisionInterval, true);
}

void AArenaAIOpponent::Think()
{
	const AArenaGameState* GameState = GetWorld()->GetGameState<AArenaGameState>();
	AArenaGameMode* GameMode = GetWorld()->GetAuthGameMode<AArenaGameMode>();
	if (!OpponentState || !GameMode || !GameState || !GameState->IsMatchRunning())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextDecisionTime)
	{
		return;
	}

	if (!PendingCard || OpponentState->FindCardInHand(PendingCard) == INDEX_NONE)
	{
		PendingCard = ChooseCard();
		FailedAttempts = 0;
	}
	// On attend d'avoir assez d'élixir pour la carte choisie.
	if (!PendingCard || OpponentState->GetElixir() < PendingCard->ElixirCost)
	{
		return;
	}

	FVector Location;
	if (ChooseLocation(PendingCard, Location) && GameMode->TryPlayCard(OpponentState, PendingCard, Location))
	{
		PendingCard = nullptr;
		NextDecisionTime = Now + FMath::FRandRange(PauseAfterPlay.X, PauseAfterPlay.Y);
	}
	else if (++FailedAttempts >= 3)
	{
		PendingCard = nullptr;
	}
}

UArenaCardData* AArenaAIOpponent::ChooseCard() const
{
	const bool bEnemyOnField = FindMostAdvancedEnemy(false) != nullptr;
	TArray<UArenaCardData*> Candidates;
	for (UArenaCardData* Card : OpponentState->GetHand())
	{
		// Un sort sans cible serait gaspillé.
		if (Card && (Card->CardType == EArenaCardType::Unit || bEnemyOnField))
		{
			Candidates.Add(Card);
		}
	}
	return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

bool AArenaAIOpponent::ChooseLocation(const UArenaCardData* Card, FVector& OutLocation) const
{
	if (Card->CardType == EArenaCardType::Spell)
	{
		const AActor* Target = FindMostAdvancedEnemy(false);
		if (!Target)
		{
			return false;
		}
		OutLocation = Target->GetActorLocation();
		return true;
	}

	const FVector Forward = ArenaLevel::GetAttackDirection(this, Team);
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
	FVector Base;
	if (const AActor* Intruder = FindMostAdvancedEnemy(true))
	{
		// Défense : entre l'intrus et nos bâtiments.
		Base = Intruder->GetActorLocation() - Forward * 150.f;
	}
	else
	{
		// Attaque : au départ du chemin d'une voie au hasard, un peu en retrait.
		const TArray<FVector> Path = ArenaLevel::GetLanePath(this, Team, FMath::RandBool() ? TEXT("gauche") : TEXT("droite"));
		if (Path.IsEmpty())
		{
			return false;
		}
		Base = Path[0] - Forward * FMath::FRandRange(0.f, 350.f);
	}

	// Quelques essais autour du point de base, jusqu'à en trouver un valable.
	for (int32 Attempt = 0; Attempt < 6; ++Attempt)
	{
		const FVector Candidate = Base + Side * FMath::FRandRange(-120.f, 120.f) - Forward * (60.f * Attempt);
		if (ArenaLevel::IsValidPlacement(this, Team, Card, Candidate, OutLocation))
		{
			return true;
		}
	}
	return false;
}

AActor* AArenaAIOpponent::FindMostAdvancedEnemy(bool bOnlyInOwnZone) const
{
	// « Avancée » vers nous : le plus loin dans le sens d'attaque de l'adversaire.
	const FVector EnemyForward = -ArenaLevel::GetAttackDirection(this, Team);
	AActor* Best = nullptr;
	double BestProgress = TNumericLimits<double>::Lowest();
	for (TActorIterator<AArenaUnit> It(GetWorld()); It; ++It)
	{
		AArenaUnit* Unit = *It;
		if (Unit->GetTeam() == Team || !ArenaCombat::IsAlive(Unit)
			|| (bOnlyInOwnZone && !ArenaLevel::IsInDeploymentZone(this, Team, Unit->GetActorLocation())))
		{
			continue;
		}
		const double Progress = FVector::DotProduct(Unit->GetActorLocation(), EnemyForward);
		if (Progress > BestProgress)
		{
			BestProgress = Progress;
			Best = Unit;
		}
	}
	return Best;
}

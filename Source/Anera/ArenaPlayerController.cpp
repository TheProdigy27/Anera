#include "ArenaPlayerController.h"
#include "Anera.h"
#include "ArenaCardData.h"
#include "ArenaGameInstance.h"
#include "ArenaGameMode.h"
#include "ArenaGameState.h"
#include "ArenaHUD.h"
#include "ArenaHUDWidget.h"
#include "ArenaLevel.h"
#include "ArenaPlayerState.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DecalActor.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Tags posés par le script d'import sur les repères du niveau.
	const FName CameraTag(TEXT("camera"));
	const FName ZoneTag(TEXT("zone"));
	const FName IntactBuildingTag(TEXT("etat:Intact"));
}

AArenaPlayerController::AArenaPlayerController()
{
	// C'est nous qui choisissons la caméra de vue : Unreal ne doit pas la remplacer automatiquement.
	bAutoManageActiveCameraTarget = false;
	// On vise le terrain avec la souris : le curseur reste visible.
	bShowMouseCursor = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	PreviewMesh = CylinderMesh.Object;
	PreviewMaterial = ShapeMaterial.Object;
}

void AArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		// Les clics vont au jeu, et la souris n'est ni capturée ni cachée.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void AArenaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// PlayerTick ne tourne que sur la machine du joueur local : chacun gère sa propre caméra et sa prévisualisation.
	UpdateArenaCamera();
	UpdatePendingCardPlays();
	// Hors du jeu (attente, écran de fin), aucune carte ne reste choisie : ni disque ni zone surlignée sous l'écran de fin.
	const AArenaGameState* ArenaGameState = GetWorld()->GetGameState<AArenaGameState>();
	if (SelectedHandSlot != INDEX_NONE && (!ArenaGameState || !ArenaGameState->IsMatchRunning()))
	{
		CancelCardSelection();
	}
	HandleCardInput();
	UpdatePlacementPreview();
	UpdateZoneHighlight();
}

void AArenaPlayerController::ServerPlayCard_Implementation(UArenaCardData* Card, FVector_NetQuantize Location, int32 RequestId)
{
	AArenaPlayerState* State = GetArenaPlayerState();
	if (AArenaGameMode* GameMode = GetWorld()->GetAuthGameMode<AArenaGameMode>())
	{
		GameMode->TryPlayCard(State, Card, Location);
	}
	// Acceptée ou refusée, la demande est traitée : le joueur l'apprend avec sa main et son élixir à jour.
	if (State)
	{
		State->ConfirmCardRequest(RequestId);
	}
}

float AArenaPlayerController::GetAvailableElixir() const
{
	const AArenaPlayerState* State = GetArenaPlayerState();
	float Elixir = State ? State->GetElixir() : 0.f;
	for (const FArenaPendingCardPlay& Pending : PendingCardPlays)
	{
		Elixir -= Pending.Card ? Pending.Card->ElixirCost : 0.f;
	}
	return FMath::Max(0.f, Elixir);
}

bool AArenaPlayerController::IsCardPending(const UArenaCardData* Card) const
{
	return Card && PendingCardPlays.ContainsByPredicate([Card](const FArenaPendingCardPlay& Pending) { return Pending.Card == Card; });
}

void AArenaPlayerController::UpdatePendingCardPlays()
{
	const AArenaPlayerState* State = GetArenaPlayerState();
	const int32 LastHandled = State ? State->GetLastCardRequestId() : 0;
	const double Now = FPlatformTime::Seconds();
	PendingCardPlays.RemoveAll([&](const FArenaPendingCardPlay& Pending)
	{
		const bool bAnswered = Pending.RequestId <= LastHandled;
		if (!bAnswered && Now - Pending.SendTime < PendingCardTimeout)
		{
			return false;
		}
		UE_LOG(LogArene, Verbose, TEXT("Carte %s : %s au bout de %.0f ms"), *GetNameSafe(Pending.Card),
			bAnswered ? TEXT("reponse du serveur") : TEXT("pas de reponse, abandon"), (Now - Pending.SendTime) * 1000.0);
		if (Pending.Marker)
		{
			Pending.Marker->Destroy();
		}
		return true;
	});
}

void AArenaPlayerController::SelectHandSlot(int32 HandIndex)
{
	const AArenaPlayerState* State = GetArenaPlayerState();
	if (!State || !State->GetHand().IsValidIndex(HandIndex) || HandIndex == SelectedHandSlot)
	{
		CancelCardSelection();
		return;
	}
	SelectedHandSlot = HandIndex;
}

void AArenaPlayerController::CancelCardSelection()
{
	SelectedHandSlot = INDEX_NONE;
	bDraggingCard = false;
}

void AArenaPlayerController::BeginCardDrag(int32 HandIndex)
{
	bDragSlotWasSelected = SelectedHandSlot == HandIndex;
	if (!bDragSlotWasSelected)
	{
		SelectHandSlot(HandIndex);
	}
	bDraggingCard = SelectedHandSlot == HandIndex;
}

void AArenaPlayerController::EndCardDrag()
{
	bDraggingCard = false;
	if (SelectedHandSlot == INDEX_NONE)
	{
		return;
	}
	if (IsCursorOverHand())
	{
		// Relâchée sur la main : c'était un clic. Un clic sur la carte déjà choisie la rend.
		if (bDragSlotWasSelected)
		{
			CancelCardSelection();
		}
		return;
	}
	// Lâchée sur le terrain : posée si le placement est valide, sinon elle revient dans la main.
	UArenaCardData* Card = nullptr;
	FVector CursorLocation;
	if (GetCardUnderCursor(Card, CursorLocation))
	{
		TryPlayCardAt(SelectedHandSlot, CursorLocation);
	}
	CancelCardSelection();
}

bool AArenaPlayerController::IsCursorOverHand() const
{
	const AArenaHUD* ArenaHUD = GetHUD<AArenaHUD>();
	const UArenaHUDWidget* HUDWidget = ArenaHUD ? ArenaHUD->GetHUDWidget() : nullptr;
	float MouseX = 0.f;
	float MouseY = 0.f;
	return HUDWidget && GetMousePosition(MouseX, MouseY) && HUDWidget->IsOverHand(FVector2D(MouseX, MouseY));
}

void AArenaPlayerController::ConfirmCardPlacement()
{
	// Un clic sur le cadre de la main (entre deux cartes) ne vise pas le terrain caché derrière.
	if (IsCursorOverHand())
	{
		return;
	}
	UArenaCardData* Card = nullptr;
	FVector CursorLocation;
	if (GetCardUnderCursor(Card, CursorLocation) && TryPlayCardAt(SelectedHandSlot, CursorLocation))
	{
		CancelCardSelection();
	}
}

bool AArenaPlayerController::TryPlayCardAt(int32 HandIndex, const FVector& Point)
{
	const AArenaPlayerState* State = GetArenaPlayerState();
	const TArray<UArenaCardData*> Hand = State ? State->GetHand() : TArray<UArenaCardData*>();
	UArenaCardData* Card = Hand.IsValidIndex(HandIndex) ? Hand[HandIndex] : nullptr;
	if (!Card)
	{
		return false;
	}
	FVector GroundLocation;
	if (IsCardPending(Card) || GetAvailableElixir() < Card->ElixirCost
		|| !ArenaLevel::IsValidPlacement(this, State->GetTeam(), Card, Point, GroundLocation))
	{
		return false;
	}

	// Après une reconnexion, le PlayerState garde le dernier numéro traité : on repart au-delà.
	LastSentCardRequestId = FMath::Max(LastSentCardRequestId, State->GetLastCardRequestId()) + 1;
	const int32 RequestId = LastSentCardRequestId;
	const double SendTime = FPlatformTime::Seconds();
	ServerPlayCard(Card, Point, RequestId);

	// Sur l'hôte, la RPC s'est exécutée sur-le-champ : la réponse est déjà là. Sinon, la carte attend la réponse.
	if (State->GetLastCardRequestId() < RequestId)
	{
		FArenaPendingCardPlay& Pending = PendingCardPlays.AddDefaulted_GetRef();
		Pending.RequestId = RequestId;
		Pending.Card = Card;
		Pending.SendTime = SendTime;
		Pending.Marker = SpawnPlacementDisc(Card, GroundLocation, PendingPreviewColor);
	}
	return true;
}

void AArenaPlayerController::HandleCardInput()
{
	// Glisser-déposer : la carte est lâchée quand le bouton gauche remonte. L'état du bouton est lu chez Slate, car
	// le relâchement peut avoir lieu au-dessus de l'interface, et le jeu ne l'apprendrait pas.
	if (bDraggingCard && FSlateApplication::IsInitialized()
		&& !FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		EndCardDrag();
	}

	// Touches de chaque emplacement de la main. Unreal reconnaît la rangée du haut au caractère qu'elle produit :
	// en AZERTY, sans Maj, elle donne & é " ' et non 1 2 3 4. Le pavé numérique marche quelle que soit la disposition.
	static const FKey SlotKeys[][3] = {
		{ EKeys::One, EKeys::Ampersand, EKeys::NumPadOne },
		{ EKeys::Two, EKeys::E_AccentAigu, EKeys::NumPadTwo },
		{ EKeys::Three, EKeys::Quote, EKeys::NumPadThree },
		{ EKeys::Four, EKeys::Apostrophe, EKeys::NumPadFour },
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotKeys); ++Index)
	{
		for (const FKey& Key : SlotKeys[Index])
		{
			if (WasInputKeyJustPressed(Key))
			{
				SelectHandSlot(Index);
				break;
			}
		}
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		CancelCardSelection();
	}
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		HandleEscape();
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		ConfirmCardPlacement();
	}
}

void AArenaPlayerController::HandleEscape()
{
	// Échap annule d'abord la carte choisie.
	if (SelectedHandSlot != INDEX_NONE)
	{
		CancelCardSelection();
		return;
	}
	// Pendant la partie, un second appui confirme : on n'abandonne pas sur une fausse manœuvre.
	const AArenaGameState* ArenaGameState = GetWorld()->GetGameState<AArenaGameState>();
	if (ArenaGameState && ArenaGameState->IsMatchRunning() && !IsQuitPending())
	{
		QuitRequestTime = GetWorld()->GetRealTimeSeconds();
		return;
	}
	if (UArenaGameInstance* GameInstance = GetGameInstance<UArenaGameInstance>())
	{
		GameInstance->ReturnToMenu();
	}
}

bool AArenaPlayerController::IsQuitPending() const
{
	return GetWorld()->GetRealTimeSeconds() - QuitRequestTime <= QuitConfirmDelay;
}

void AArenaPlayerController::RequestRematch()
{
	// Chez l'hôte, la RPC s'exécute tout de suite ; chez un client, elle part au serveur.
	ServerRequestRematch();
}

void AArenaPlayerController::ServerRequestRematch_Implementation()
{
	if (AArenaGameMode* GameMode = GetWorld()->GetAuthGameMode<AArenaGameMode>())
	{
		GameMode->RequestRematch(GetArenaPlayerState());
	}
}

bool AArenaPlayerController::GetCardUnderCursor(UArenaCardData*& OutCard, FVector& OutCursorLocation) const
{
	const AArenaPlayerState* State = GetArenaPlayerState();
	const TArray<UArenaCardData*> Hand = State ? State->GetHand() : TArray<UArenaCardData*>();
	if (!Hand.IsValidIndex(SelectedHandSlot) || !Hand[SelectedHandSlot])
	{
		return false;
	}

	// Seul le décor statique compte (sol marchable, murs, bâtiments) : on ne vise pas à travers les unités.
	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { UEngineTypes::ConvertToObjectType(ECC_WorldStatic) };
	FHitResult Hit;
	if (!GetHitResultUnderCursorForObjects(ObjectTypes, false, Hit))
	{
		return false;
	}
	OutCard = Hand[SelectedHandSlot];
	OutCursorLocation = Hit.Location;
	return true;
}

void AArenaPlayerController::UpdatePlacementPreview()
{
	UArenaCardData* Card = nullptr;
	FVector CursorLocation;
	const AArenaPlayerState* State = GetArenaPlayerState();
	// Au-dessus de la main, la souris ne vise pas le terrain : pas de disque.
	if (!State || IsCursorOverHand() || !GetCardUnderCursor(Card, CursorLocation))
	{
		if (PreviewActor)
		{
			PreviewActor->SetActorHiddenInGame(true);
		}
		return;
	}

	FVector GroundLocation = CursorLocation;
	const bool bValid = !IsCardPending(Card) && GetAvailableElixir() >= Card->ElixirCost
		&& ArenaLevel::IsValidPlacement(this, State->GetTeam(), Card, CursorLocation, GroundLocation);
	const FLinearColor& Color = bValid ? ValidPreviewColor : InvalidPreviewColor;

	if (!PreviewActor)
	{
		PreviewActor = SpawnPlacementDisc(Card, GroundLocation, Color);
		return;
	}
	PlaceDisc(PreviewActor, Card, GroundLocation, Color);
	PreviewActor->SetActorHiddenInGame(false);
}

AStaticMeshActor* AArenaPlayerController::SpawnPlacementDisc(const UArenaCardData* Card, const FVector& GroundLocation, const FLinearColor& Color)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	AStaticMeshActor* Disc = GetWorld()->SpawnActor<AStaticMeshActor>(GroundLocation, FRotator::ZeroRotator, SpawnParams);
	if (!Disc)
	{
		return nullptr;
	}
	Disc->SetReplicates(false);
	Disc->SetActorEnableCollision(false);
	UStaticMeshComponent* MeshComponent = Disc->GetStaticMeshComponent();
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetStaticMesh(PreviewMesh);
	MeshComponent->SetCastShadow(false);
	MeshComponent->SetReceivesDecals(false);
	MeshComponent->CreateDynamicMaterialInstance(0, PreviewMaterial);
	PlaceDisc(Disc, Card, GroundLocation, Color);
	return Disc;
}

void AArenaPlayerController::PlaceDisc(AStaticMeshActor* Disc, const UArenaCardData* Card, const FVector& GroundLocation, const FLinearColor& Color)
{
	// Disque plat : rayon du sort, ou place occupée par les unités.
	const float Radius = Card->CardType == EArenaCardType::Spell ? Card->SpellRadius : (Card->UnitCount > 1 ? 90.f : 50.f);
	Disc->SetActorLocation(GroundLocation + FVector(0.f, 0.f, 3.f));
	Disc->SetActorScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.04f));
	if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Disc->GetStaticMeshComponent()->GetMaterial(0)))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AArenaPlayerController::UpdateZoneHighlight()
{
	// Une carte d'unité est choisie (au clavier, au clic ou en la glissant) : seules les unités ont une zone, les sorts
	// visent n'importe où.
	const AArenaPlayerState* State = GetArenaPlayerState();
	const TArray<UArenaCardData*> Hand = State ? State->GetHand() : TArray<UArenaCardData*>();
	const UArenaCardData* Card = Hand.IsValidIndex(SelectedHandSlot) ? Hand[SelectedHandSlot] : nullptr;
	const EArenaTeam Team = State ? State->GetTeam() : EArenaTeam::None;
	const bool bShow = Card && Card->CardType == EArenaCardType::Unit && Team != EArenaTeam::None && DeploymentZoneMaterial;
	if (!bShow)
	{
		if (ZoneHighlight)
		{
			ZoneHighlight->SetActorHiddenInGame(true);
		}
		return;
	}

	if (!ZoneHighlight || ZoneHighlightTeam != Team)
	{
		const AActor* Zone = ArenaLevel::FindDeploymentZone(this, Team);
		const FBox Box = Zone ? Zone->GetComponentsBoundingBox(true) : FBox(ForceInit);
		if (!Box.IsValid)
		{
			return;
		}
		if (!ZoneHighlight)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			// Un décal projette son matériau sur tout ce qui se trouve dans sa boîte, le long de son axe X : tourné vers
			// le bas, il colore le sol en suivant le relief. ADecalActor l'oriente déjà vers le bas (-90° de tangage) ;
			// SpawnActor compose la rotation donnée avec celle-là, d'où une rotation nulle ici.
			ZoneHighlight = GetWorld()->SpawnActor<ADecalActor>(Box.GetCenter(), FRotator::ZeroRotator, SpawnParams);
			if (!ZoneHighlight)
			{
				return;
			}
			UDecalComponent* Decal = ZoneHighlight->GetDecal();
			Decal->SetDecalMaterial(DeploymentZoneMaterial);
			if (UMaterialInstanceDynamic* Material = Decal->CreateDynamicMaterialInstance())
			{
				Material->SetVectorParameterValue(TEXT("Color"), DeploymentZoneColor);
			}
			// Toujours affiché, même quand il occupe peu de place à l'écran.
			Decal->SetFadeScreenSize(0.f);
		}
		// Taille du décal : X = demi-profondeur de projection (le sol, les socles et le bas des bâtiments),
		// Y et Z = demi-largeurs au sol. Avec la rotation vers le bas, Y reste l'axe Y du monde et Z devient l'axe X.
		const FVector Extent = Box.GetExtent();
		ZoneHighlight->SetActorLocation(Box.GetCenter());
		ZoneHighlight->GetDecal()->DecalSize = FVector(Extent.Z + 300.f, Extent.Y, Extent.X);
		ZoneHighlight->GetDecal()->MarkRenderStateDirty();
		ZoneHighlightTeam = Team;
	}
	ZoneHighlight->SetActorHiddenInGame(false);
}

AArenaPlayerState* AArenaPlayerController::GetArenaPlayerState() const
{
	return GetPlayerState<AArenaPlayerState>();
}

void AArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ArenaCamera)
	{
		ArenaCamera->Destroy();
		ArenaCamera = nullptr;
	}
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (ZoneHighlight)
	{
		ZoneHighlight->Destroy();
		ZoneHighlight = nullptr;
	}
	for (const FArenaPendingCardPlay& Pending : PendingCardPlays)
	{
		if (Pending.Marker)
		{
			Pending.Marker->Destroy();
		}
	}
	PendingCardPlays.Reset();
	Super::EndPlay(EndPlayReason);
}

void AArenaPlayerController::UpdateArenaCamera()
{
	if (bReferencesMissing)
	{
		return;
	}

	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return;
	}

	// Chez un client, le camp arrive du serveur par réplication : on attend de le connaître.
	const APlayerState* State = GetPlayerState<APlayerState>();
	if (!State)
	{
		return;
	}
	const AArenaPlayerState* ArenaState = Cast<AArenaPlayerState>(State);
	if (ArenaState && ArenaState->GetTeam() == EArenaTeam::None)
	{
		return;
	}
	const EArenaTeam Team = ArenaState ? ArenaState->GetTeam() : EArenaTeam::J;

	const FIntPoint ViewportSize(SizeX, SizeY);
	if (ArenaCamera && Team == CameraTeam && ViewportSize == CameraViewportSize)
	{
		return;
	}

	if (!SourceCamera && !GatherArenaReferences())
	{
		bReferencesMissing = true;
		return;
	}

	// Même orientation que Camera_Arene ; le camp A regarde depuis l'autre bout du terrain.
	FRotator Rotation = SourceCamera->GetActorRotation();
	if (Team == EArenaTeam::A)
	{
		Rotation.Yaw += 180.f;
	}

	const double TanHalfVertical = FMath::Tan(FMath::DegreesToRadians(VerticalFieldOfView * 0.5));
	const double TanHalfHorizontal = TanHalfVertical * SizeX / SizeY;
	const FVector Location = ComputeFittedLocation(Rotation, TanHalfHorizontal, TanHalfVertical);

	if (!ArenaCamera)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ArenaCamera = GetWorld()->SpawnActor<ACameraActor>(Location, Rotation, SpawnParams);
		if (!ArenaCamera)
		{
			return;
		}
		ArenaCamera->SetReplicates(false);

		// Mêmes réglages d'image (post-process) que la caméra importée.
		const UCameraComponent* SourceComponent = SourceCamera->GetCameraComponent();
		UCameraComponent* Component = ArenaCamera->GetCameraComponent();
		Component->PostProcessSettings = SourceComponent->PostProcessSettings;
		Component->PostProcessBlendWeight = SourceComponent->PostProcessBlendWeight;

		// Le FieldOfView d'Unreal est l'angle horizontal : on le recalcule nous-mêmes pour chaque format d'écran.
		Component->SetConstraintAspectRatio(false);
		Component->bOverrideAspectRatioAxisConstraint = true;
		Component->AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
	}
	else
	{
		ArenaCamera->SetActorLocationAndRotation(Location, Rotation);
	}

	UCameraComponent* Component = ArenaCamera->GetCameraComponent();
	Component->SetFieldOfView(FMath::RadiansToDegrees(2.0 * FMath::Atan(TanHalfHorizontal)));
	if (GetViewTarget() != ArenaCamera)
	{
		SetViewTargetWithBlend(ArenaCamera, 0.f);
	}

	CameraTeam = Team;
	CameraViewportSize = ViewportSize;
	UE_LOG(LogArene, Log, TEXT("Camera du camp %s : ecran %dx%d, position %s, FOV horizontal %.1f"),
		Team == EArenaTeam::A ? TEXT("A") : TEXT("J"), SizeX, SizeY, *Location.ToCompactString(), Component->FieldOfView);
}

bool AArenaPlayerController::GatherArenaReferences()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, CameraTag, Found);
	for (AActor* Actor : Found)
	{
		if (ACameraActor* Camera = Cast<ACameraActor>(Actor))
		{
			SourceCamera = Camera;
			break;
		}
	}
	if (!SourceCamera)
	{
		UE_LOG(LogArene, Warning, TEXT("Aucune CameraActor avec le tag '%s' : camera de l'arene non configuree."), *CameraTag.ToString());
		return false;
	}

	FramingPoints.Reset();

	// Zones de déploiement : le rectangle de sol qu'elles couvrent, pris à leur hauteur.
	UGameplayStatics::GetAllActorsWithTag(this, ZoneTag, Found);
	for (const AActor* Zone : Found)
	{
		const FBox Box = Zone->GetComponentsBoundingBox(true);
		if (!Box.IsValid)
		{
			continue;
		}
		const double Z = Box.GetCenter().Z;
		FramingPoints.Add(FVector(Box.Min.X, Box.Min.Y, Z));
		FramingPoints.Add(FVector(Box.Min.X, Box.Max.Y, Z));
		FramingPoints.Add(FVector(Box.Max.X, Box.Min.Y, Z));
		FramingPoints.Add(FVector(Box.Max.X, Box.Max.Y, Z));
	}

	// Bâtiments intacts : leur volume entier, pour que le haut des tours reste dans l'image.
	UGameplayStatics::GetAllActorsWithTag(this, IntactBuildingTag, Found);
	for (const AActor* Building : Found)
	{
		const FBox Box = Building->GetComponentsBoundingBox(true);
		if (!Box.IsValid)
		{
			continue;
		}
		FVector Corners[8];
		Box.GetVertices(Corners);
		FramingPoints.Append(Corners, UE_ARRAY_COUNT(Corners));
	}

	if (FramingPoints.IsEmpty())
	{
		UE_LOG(LogArene, Warning, TEXT("Aucun acteur avec le tag '%s' ou '%s' : impossible de cadrer l'arene."),
			*ZoneTag.ToString(), *IntactBuildingTag.ToString());
		return false;
	}
	return true;
}

FVector AArenaPlayerController::ComputeFittedLocation(const FRotator& Rotation, double TanHalfHorizontal, double TanHalfVertical) const
{
	const FRotationMatrix Axes(Rotation);
	const FVector Forward = Axes.GetUnitAxis(EAxis::X);
	const FVector Right = Axes.GetUnitAxis(EAxis::Y);
	const FVector Up = Axes.GetUnitAxis(EAxis::Z);

	// Pente de chaque bord de l'image, marge comprise.
	const double SlopeLeft = TanHalfHorizontal * (1.0 - 2.0 * ScreenMarginLeft);
	const double SlopeRight = TanHalfHorizontal * (1.0 - 2.0 * ScreenMarginRight);
	const double SlopeTop = TanHalfVertical * (1.0 - 2.0 * ScreenMarginTop);
	const double SlopeBottom = TanHalfVertical * (1.0 - 2.0 * ScreenMarginBottom);

	// Vu depuis la caméra, un point est à l'image si son écart au centre ne dépasse pas
	// pente × profondeur. Pour chaque bord, on retient le point le plus contraignant.
	double MaxRight = TNumericLimits<double>::Lowest();
	double MaxLeft = TNumericLimits<double>::Lowest();
	double MaxTop = TNumericLimits<double>::Lowest();
	double MaxBottom = TNumericLimits<double>::Lowest();
	for (const FVector& Point : FramingPoints)
	{
		const double Depth = Point | Forward;
		const double Side = Point | Right;
		const double Height = Point | Up;
		MaxRight = FMath::Max(MaxRight, Side - SlopeRight * Depth);
		MaxLeft = FMath::Max(MaxLeft, -Side - SlopeLeft * Depth);
		MaxTop = FMath::Max(MaxTop, Height - SlopeTop * Depth);
		MaxBottom = FMath::Max(MaxBottom, -Height - SlopeBottom * Depth);
	}

	// Recul nécessaire pour la largeur et pour la hauteur : on garde le plus grand des deux.
	const double DepthForWidth = -(MaxRight + MaxLeft) / (SlopeRight + SlopeLeft);
	const double DepthForHeight = -(MaxTop + MaxBottom) / (SlopeTop + SlopeBottom);
	const double CameraDepth = FMath::Min(DepthForWidth, DepthForHeight);

	// Sur l'axe qui a du jeu, l'arène est centrée entre les marges.
	const double CameraSide = 0.5 * ((MaxRight + SlopeRight * CameraDepth) - (MaxLeft + SlopeLeft * CameraDepth));
	const double CameraHeight = 0.5 * ((MaxTop + SlopeTop * CameraDepth) - (MaxBottom + SlopeBottom * CameraDepth));

	return Forward * CameraDepth + Right * CameraSide + Up * CameraHeight;
}

namespace
{
	// Joueur local qui jouera la carte. Sans camp précisé : celui du monde de la console.
	// Avec un camp : le joueur local de ce camp, cherché dans tous les mondes du processus. En PIE, l'hôte et
	// chaque client ont leur propre monde, et la console de l'éditeur vise toujours l'hôte : c'est ce qui permet
	// de jouer pour un client.
	AArenaPlayerController* FindLocalController(UWorld* ConsoleWorld, EArenaTeam Team)
	{
		if (!GEngine)
		{
			return nullptr;
		}
		if (Team == EArenaTeam::None)
		{
			return ConsoleWorld ? Cast<AArenaPlayerController>(GEngine->GetFirstLocalPlayerController(ConsoleWorld)) : nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			AArenaPlayerController* Controller = World && World->IsGameWorld()
				? Cast<AArenaPlayerController>(GEngine->GetFirstLocalPlayerController(World)) : nullptr;
			const AArenaPlayerState* State = Controller ? Controller->GetPlayerState<AArenaPlayerState>() : nullptr;
			if (State && State->GetTeam() == Team)
			{
				return Controller;
			}
		}
		return nullptr;
	}

	// Commande de test, pour poser une carte sans souris (réseau, automatisation) :
	// « Arene.PoseCarte <emplacement 1-4> <X> <Y> [J|A] » fait comme un clic au point (X, Y) du terrain,
	// avec les mêmes vérifications chez le joueur, puis ServerPlayCard.
	void PlayCardFromConsole(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		const FString TeamArg = Args.IsValidIndex(3) ? Args[3] : FString();
		const EArenaTeam Team = TeamArg.Equals(TEXT("J"), ESearchCase::IgnoreCase) ? EArenaTeam::J
			: TeamArg.Equals(TEXT("A"), ESearchCase::IgnoreCase) ? EArenaTeam::A
			: EArenaTeam::None;
		AArenaPlayerController* Controller = FindLocalController(ConsoleWorld, Team);
		if (!Controller || Args.Num() < 3)
		{
			UE_LOG(LogArene, Warning, TEXT("Usage : Arene.PoseCarte <emplacement 1-4> <X> <Y> [J|A] (aucun joueur local trouve pour ce camp ?)"));
			return;
		}
		const int32 HandIndex = FCString::Atoi(*Args[0]) - 1;
		const double X = FCString::Atod(*Args[1]);
		const double Y = FCString::Atod(*Args[2]);

		// Point du terrain à la verticale de (X, Y), sur le décor statique comme sous la souris.
		UWorld* World = Controller->GetWorld();
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByObjectType(Hit, FVector(X, Y, 5000.0), FVector(X, Y, -5000.0),
			FCollisionObjectQueryParams(ECC_WorldStatic));
		const bool bSent = bHit && Controller->TryPlayCardAt(HandIndex, Hit.Location);
		UE_LOG(LogArene, Log, TEXT("Arene.PoseCarte (%s) : emplacement %d en %s : %s"),
			World->GetNetMode() == NM_Client ? TEXT("client") : TEXT("serveur"), HandIndex + 1,
			bHit ? *Hit.Location.ToCompactString() : TEXT("(hors du terrain)"),
			bSent ? TEXT("envoyee au serveur") : TEXT("refusee par le joueur"));
	}

	FAutoConsoleCommandWithWorldAndArgs PlayCardCommand(
		TEXT("Arene.PoseCarte"),
		TEXT("Arene.PoseCarte <emplacement 1-4> <X> <Y> [J|A] : pose une carte de la main au point (X, Y) du terrain, ")
		TEXT("pour le joueur local du camp indiqué (test sans souris)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PlayCardFromConsole));

	// Commande de test : « Arene.QuitterPartie [J|A] » ramène au menu le joueur local de ce camp, comme Échap
	// (en PIE, Échap arrête la session de l'éditeur au lieu d'arriver au jeu).
	void QuitMatchFromConsole(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		const FString TeamArg = Args.IsValidIndex(0) ? Args[0] : FString();
		const EArenaTeam Team = TeamArg.Equals(TEXT("J"), ESearchCase::IgnoreCase) ? EArenaTeam::J
			: TeamArg.Equals(TEXT("A"), ESearchCase::IgnoreCase) ? EArenaTeam::A
			: EArenaTeam::None;
		const AArenaPlayerController* Controller = FindLocalController(ConsoleWorld, Team);
		UArenaGameInstance* GameInstance = Controller ? Controller->GetGameInstance<UArenaGameInstance>() : nullptr;
		if (!GameInstance)
		{
			UE_LOG(LogArene, Warning, TEXT("Usage : Arene.QuitterPartie [J|A] (aucun joueur local trouve pour ce camp ?)"));
			return;
		}
		UE_LOG(LogArene, Log, TEXT("Arene.QuitterPartie : retour au menu du joueur %s"), *Controller->GetName());
		GameInstance->ReturnToMenu();
	}

	FAutoConsoleCommandWithWorldAndArgs QuitMatchCommand(
		TEXT("Arene.QuitterPartie"),
		TEXT("Arene.QuitterPartie [J|A] : ramène au menu le joueur local de ce camp, comme Échap (test)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&QuitMatchFromConsole));
}

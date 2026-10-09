#include "ArenaPlayerController.h"
#include "Anera.h"
#include "ArenaPlayerState.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

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
}

void AArenaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// PlayerTick ne tourne que sur la machine du joueur local : chacun gère sa propre caméra.
	UpdateArenaCamera();
}

void AArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ArenaCamera)
	{
		ArenaCamera->Destroy();
		ArenaCamera = nullptr;
	}
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

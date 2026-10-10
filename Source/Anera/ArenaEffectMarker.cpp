#include "ArenaEffectMarker.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Taille de départ d'un effet instantané, qui grandit ensuite jusqu'à sa taille réelle.
	constexpr float FlashStartScale = 0.3f;
}

AArenaEffectMarker::AArenaEffectMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	HitShape = SphereMesh.Object;
	ZoneShape = CylinderMesh.Object;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visuel"));
	RootComponent = Mesh;
	Mesh->SetStaticMesh(HitShape);
	Mesh->SetMaterial(0, ShapeMaterial.Object);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
}

void AArenaEffectMarker::InitializeMarker(const FArenaEffect& Effect)
{
	FVector Location = Effect.Location;
	if (Effect.Radius > 0.f)
	{
		// Une zone se dessine à plat sur le sol, sous le point d'impact (le sol marchable est en WorldStatic).
		Mesh->SetStaticMesh(ZoneShape);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0.0, 0.0, 50.0), Location - FVector(0.0, 0.0, 1000.0),
			FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			Location = Hit.Location;
		}
		// Le cylindre du moteur fait 100 × 100 × 100 et il est centré : disque de 4 cm d'épaisseur posé sur le sol.
		FullScale = FVector(Effect.Radius / 50.f, Effect.Radius / 50.f, 0.04f);
		Location.Z += 2.0;
	}
	else
	{
		FullScale = FVector(HitSize / 100.f);
	}
	SetActorLocation(Location);

	if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Effect.Team == EArenaTeam::A ? TeamAColor : TeamJColor);
	}

	const bool bLasting = Effect.Duration > 0.f;
	Mesh->SetRelativeScale3D(bLasting ? FullScale : FullScale * FlashStartScale);
	SetActorTickEnabled(!bLasting);
	SetLifeSpan(bLasting ? Effect.Duration : FlashDuration);
}

void AArenaEffectMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / FlashDuration, 0.f, 1.f);
	Mesh->SetRelativeScale3D(FullScale * FMath::Lerp(FlashStartScale, 1.f, Alpha));
}

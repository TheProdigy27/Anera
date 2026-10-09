#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetSerialization.h"
#include "ArenaTypes.h"
#include "ArenaPlayerController.generated.h"

class AArenaPlayerState;
class ACameraActor;
class AStaticMeshActor;
class UArenaCardData;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

// Contrôleur du joueur : la caméra de l'arène et la pose des cartes.
//
// La caméra reprend l'orientation de Camera_Arene (trouvée par son tag « camera »), tournée de 180°
// pour le camp A, puis se place pour que les zones de déploiement et les bâtiments tiennent dans
// l'écran, quel que soit son format. Camera_Arene elle-même n'est jamais modifiée.
//
// Pose des cartes, en attendant l'interface (phase 3) : touches 1 à 4 pour choisir une carte de la main,
// un disque suit la souris (vert : placement valide, rouge : refusé), clic gauche pour poser,
// clic droit ou Échap pour annuler. Le placement est vérifié ici pour l'affichage, puis revérifié par le serveur.
UCLASS()
class ANERA_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	// RPC « Server » : appelée sur la machine du joueur, exécutée sur le serveur, qui revérifie tout.
	// Le client n'envoie qu'une intention ; Reliable garantit qu'elle arrive.
	UFUNCTION(Server, Reliable)
	void ServerPlayCard(UArenaCardData* Card, FVector_NetQuantize Location);

	// Choisit la carte à cet emplacement de la main (0 à 3). Rechoisir la même carte annule.
	UFUNCTION(BlueprintCallable, Category = "Arene|Cartes")
	void SelectHandSlot(int32 HandIndex);

	UFUNCTION(BlueprintCallable, Category = "Arene|Cartes")
	void CancelCardSelection();

	// Pose la carte choisie sous la souris, si le placement est valide.
	UFUNCTION(BlueprintCallable, Category = "Arene|Cartes")
	void ConfirmCardPlacement();

	// Emplacement de la carte choisie dans la main, ou INDEX_NONE.
	UFUNCTION(BlueprintPure, Category = "Arene|Cartes")
	int32 GetSelectedHandSlot() const { return SelectedHandSlot; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Disque de prévisualisation (forme et matériau simples du moteur, teinté par le paramètre « Color »).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Cartes")
	TObjectPtr<UStaticMesh> PreviewMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Cartes")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Cartes")
	FLinearColor ValidPreviewColor = FLinearColor(0.1f, 0.9f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Cartes")
	FLinearColor InvalidPreviewColor = FLinearColor(0.9f, 0.1f, 0.1f);

	// Champ de vision vertical, en degrés. Le champ horizontal découle du format de l'écran.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Camera", meta = (ClampMin = "10", ClampMax = "120"))
	float VerticalFieldOfView = 45.f;

	// Marges laissées autour de l'arène, en fraction de l'écran (0,03 = 3 %).
	// Servira à dégager la place de l'interface (phase 3).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Camera", meta = (ClampMin = "0", ClampMax = "0.4"))
	float ScreenMarginLeft = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Camera", meta = (ClampMin = "0", ClampMax = "0.4"))
	float ScreenMarginRight = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Camera", meta = (ClampMin = "0", ClampMax = "0.4"))
	float ScreenMarginTop = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Camera", meta = (ClampMin = "0", ClampMax = "0.4"))
	float ScreenMarginBottom = 0.03f;

private:
	// Touches de la pose de cartes (lues à chaque image).
	void HandleCardInput();

	// Place, colore ou cache le disque de prévisualisation.
	void UpdatePlacementPreview();

	// Carte choisie et point du sol sous la souris. Faux si rien n'est choisi ou si la souris est hors du terrain.
	bool GetCardUnderCursor(UArenaCardData*& OutCard, FVector& OutCursorLocation) const;

	AArenaPlayerState* GetArenaPlayerState() const;

	// Recadre la caméra si le camp ou la taille de la fenêtre ont changé.
	void UpdateArenaCamera();

	// Lit les repères du niveau : orientation de Camera_Arene et points à faire tenir à l'écran.
	bool GatherArenaReferences();

	// Position de caméra qui fait tenir tous les points de cadrage, à orientation et champ de vision donnés.
	FVector ComputeFittedLocation(const FRotator& Rotation, double TanHalfHorizontal, double TanHalfVertical) const;

	// Caméra locale à ce joueur (jamais répliquée).
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ArenaCamera;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> SourceCamera;

	TArray<FVector> FramingPoints;
	bool bReferencesMissing = false;

	EArenaTeam CameraTeam = EArenaTeam::None;
	FIntPoint CameraViewportSize = FIntPoint::ZeroValue;

	int32 SelectedHandSlot = INDEX_NONE;

	// Disque de prévisualisation, local à ce joueur (jamais répliqué).
	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewMaterialInstance;
};

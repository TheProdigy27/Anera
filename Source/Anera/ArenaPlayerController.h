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

// Carte envoyée au serveur, dont la réponse n'est pas encore arrivée chez ce joueur.
USTRUCT()
struct FArenaPendingCardPlay
{
	GENERATED_BODY()

	int32 RequestId = 0;

	UPROPERTY()
	TObjectPtr<UArenaCardData> Card;

	// Heure d'envoi (temps réel), pour mesurer l'attente et abandonner une demande restée sans réponse.
	double SendTime = 0.0;

	// Disque posé à l'endroit visé, le temps que la réponse arrive.
	UPROPERTY()
	TObjectPtr<AStaticMeshActor> Marker;
};

// Contrôleur du joueur : la caméra de l'arène et la pose des cartes.
//
// La caméra reprend l'orientation de Camera_Arene (trouvée par son tag « camera »), tournée de 180°
// pour le camp A, puis se place pour que les zones de déploiement et les bâtiments tiennent dans
// l'écran, quel que soit son format. Camera_Arene elle-même n'est jamais modifiée.
//
// Pose des cartes : touches 1 à 4 ou clic sur une carte du HUD (UArenaCardWidget) pour choisir une carte de la main,
// un disque suit la souris (vert : placement valide, rouge : refusé), clic gauche pour poser,
// clic droit ou Échap pour annuler. Le placement est vérifié ici pour l'affichage, puis revérifié par le serveur.
//
// En réseau, la réponse du serveur arrive après un aller-retour (100 à 300 ms par Internet). En attendant, la carte
// envoyée est « en attente » : son coût est déjà déduit de l'élixir affiché, elle ne peut pas être rejouée, et un
// disque blanc marque l'endroit visé. Le serveur renvoie le numéro de la demande avec la main et l'élixir à jour
// (AArenaPlayerState::GetLastCardRequestId) : l'attente prend fin quand ce numéro arrive, carte acceptée ou refusée.
//
// Échap sans carte choisie revient au menu : tout de suite avant ou après la partie, après un second appui pendant.
UCLASS()
class ANERA_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	// RPC « Server » : appelée sur la machine du joueur, exécutée sur le serveur, qui revérifie tout.
	// Le client n'envoie qu'une intention ; Reliable garantit qu'elle arrive. RequestId : numéro de la demande,
	// renvoyé par le serveur une fois traitée.
	UFUNCTION(Server, Reliable)
	void ServerPlayCard(UArenaCardData* Card, FVector_NetQuantize Location, int32 RequestId);

	// Choisit la carte à cet emplacement de la main (0 à 3). Rechoisir la même carte annule.
	UFUNCTION(BlueprintCallable, Category = "Arene|Cartes")
	void SelectHandSlot(int32 HandIndex);

	UFUNCTION(BlueprintCallable, Category = "Arene|Cartes")
	void CancelCardSelection();

	// Pose la carte choisie sous la souris, si le placement est valide.
	UFUNCTION(BlueprintCallable, Category = "Arene|Cartes")
	void ConfirmCardPlacement();

	// Pose la carte de cet emplacement de la main au point visé : vérification locale (élixir, zone),
	// puis envoi au serveur. Faux si le placement est refusé ici. Sert au clic et à la commande Arene.PoseCarte.
	bool TryPlayCardAt(int32 HandIndex, const FVector& Point);

	// Emplacement de la carte choisie dans la main, ou INDEX_NONE.
	UFUNCTION(BlueprintPure, Category = "Arene|Cartes")
	int32 GetSelectedHandSlot() const { return SelectedHandSlot; }

	// Élixir du joueur, moins le coût des cartes envoyées dont la réponse n'est pas arrivée. C'est la valeur à afficher.
	UFUNCTION(BlueprintPure, Category = "Arene|Cartes")
	float GetAvailableElixir() const;

	// Vrai si cette carte est envoyée au serveur et attend sa réponse.
	UFUNCTION(BlueprintPure, Category = "Arene|Cartes")
	bool IsCardPending(const UArenaCardData* Card) const;

	// Vrai juste après un premier appui sur Échap pendant la partie : un second appui ramène au menu.
	UFUNCTION(BlueprintPure, Category = "Arene|Menu")
	bool IsQuitPending() const;

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

	// Disque d'une carte envoyée, en attente de la réponse du serveur.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Cartes")
	FLinearColor PendingPreviewColor = FLinearColor(0.9f, 0.9f, 0.9f);

	// Une demande restée sans réponse au bout de ce délai est abandonnée (serveur perdu).
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Cartes", meta = (ClampMin = "1", Units = "s"))
	float PendingCardTimeout = 5.f;

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

	// Délai pour confirmer l'abandon de la partie par un second appui sur Échap.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Menu", meta = (ClampMin = "0.5", Units = "s"))
	float QuitConfirmDelay = 3.f;

private:
	// Touches de la pose de cartes (lues à chaque image).
	void HandleCardInput();

	// Échap : annule la carte choisie, sinon ramène au menu (avec confirmation pendant la partie).
	void HandleEscape();

	// Place, colore ou cache le disque de prévisualisation.
	void UpdatePlacementPreview();

	// Retire les cartes en attente dont la réponse est arrivée (ou trop ancienne), avec leur disque.
	void UpdatePendingCardPlays();

	// Disque plat, local à ce joueur, de la taille occupée par la carte au sol. Null si l'apparition échoue.
	AStaticMeshActor* SpawnPlacementDisc(const UArenaCardData* Card, const FVector& GroundLocation, const FLinearColor& Color);

	// Place, met à l'échelle et colore un disque.
	static void PlaceDisc(AStaticMeshActor* Disc, const UArenaCardData* Card, const FVector& GroundLocation, const FLinearColor& Color);

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

	// Heure (temps réel) du premier appui sur Échap pendant la partie.
	double QuitRequestTime = -1.0e9;

	// Disque de prévisualisation, local à ce joueur (jamais répliqué).
	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> PreviewActor;

	// Cartes envoyées au serveur, dans l'ordre d'envoi.
	UPROPERTY(Transient)
	TArray<FArenaPendingCardPlay> PendingCardPlays;

	// Numéro de la dernière demande envoyée.
	int32 LastSentCardRequestId = 0;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetSerialization.h"
#include "ArenaTypes.h"
#include "ArenaPlayerController.generated.h"

class ACameraActor;
class UArenaCardData;

// Contrôleur du joueur : la caméra de l'arène et l'envoi des cartes jouées au serveur
// (sélection, glisser-déposer et prévisualisation arrivent en 1.2.4).
//
// La caméra reprend l'orientation de Camera_Arene (trouvée par son tag « camera »), tournée de 180°
// pour le camp A, puis se place pour que les zones de déploiement et les bâtiments tiennent dans
// l'écran, quel que soit son format. Camera_Arene elle-même n'est jamais modifiée.
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

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
};

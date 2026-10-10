#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArenaMenuPlayerController.generated.h"

class UArenaMenuWidget;

// Contrôleur du joueur sur la carte du menu : affiche le menu et donne la souris et le clavier à l'interface.
UCLASS()
class ANERA_API AArenaMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaMenuPlayerController();

protected:
	virtual void BeginPlay() override;

	// Widget du menu : un Widget Blueprint enfant d'UArenaMenuWidget (WBP_MenuPrincipal), réglé dans le Blueprint du contrôleur.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Menu")
	TSubclassOf<UArenaMenuWidget> MenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UArenaMenuWidget> MenuWidget;
};

#include "ArenaMenuPlayerController.h"
#include "Anera.h"
#include "ArenaMenuWidget.h"

AArenaMenuPlayerController::AArenaMenuPlayerController()
{
	bShowMouseCursor = true;
}

void AArenaMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	if (!MenuWidgetClass)
	{
		UE_LOG(LogArene, Warning, TEXT("%s : MenuWidgetClass n'est pas reglee, aucun menu a afficher."), *GetClass()->GetName());
		return;
	}

	// CreateWidget fabrique le widget pour ce joueur ; AddToViewport l'affiche par-dessus la vue 3D.
	MenuWidget = CreateWidget<UArenaMenuWidget>(this, MenuWidgetClass);
	if (!MenuWidget)
	{
		return;
	}
	MenuWidget->AddToViewport();

	// Interface seule : clics et touches vont au menu, rien au jeu. Le menu doit pouvoir recevoir le focus clavier.
	MenuWidget->SetIsFocusable(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

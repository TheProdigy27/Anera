#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "ArenaMenuWidget.generated.h"

class UArenaGameInstance;
class UButton;
class UEditableTextBox;
class UTextBlock;

// Menu d'accueil : Solo contre l'IA, Héberger, Rejoindre (adresse IP de l'hôte), Quitter.
// La mise en page est faite dans le Widget Blueprint WBP_MenuPrincipal (enfant de cette classe) ; le C++ relie
// ses boutons aux actions de UArenaGameInstance.
//
// meta = (BindWidget) : le Widget Blueprint doit contenir un widget de ce type et portant exactement ce nom,
// sinon il ne compile pas. Le C++ reçoit ainsi ses widgets sans les chercher.
UCLASS(Abstract)
class ANERA_API UArenaMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Appelée une seule fois, à la création du widget : on y branche les boutons.
	virtual void NativeOnInitialized() override;
	// Appelée à chaque ajout à l'écran : on y affiche l'adresse mémorisée et le message laissé par une erreur réseau.
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SoloButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> JoinButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuitButton;

	// Adresse de l'hôte : « 192.168.1.20 » ou « 192.168.1.20:7777 ». Entrée lance la connexion.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> AddressBox;

	// Message d'état (connexion en cours) ou d'erreur (partie complète, hôte injoignable…).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Menu")
	FSlateColor InfoColor = FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f));

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Menu")
	FSlateColor ErrorColor = FSlateColor(FLinearColor(1.f, 0.45f, 0.35f));

private:
	UFUNCTION()
	void HandleSoloClicked();

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void ShowMessage(const FText& Message, bool bError);

	UArenaGameInstance* GetArenaGameInstance() const;
};

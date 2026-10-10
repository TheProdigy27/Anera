#pragma once

#include "CoreMinimal.h"
#include "ArenaSessionSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Types/SlateEnums.h"
#include "ArenaMenuWidget.generated.h"

class UArenaGameInstance;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;

// Bouton d'une partie de la liste, créé par le C++ : il donne son numéro au clic, ce que l'événement OnClicked
// d'un UButton ne fait pas (il ne dit pas quel bouton a été cliqué).
UCLASS()
class ANERA_API UArenaSessionButton : public UButton
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnChosen, int32 /*SessionIndex*/);

	void Setup(int32 InSessionIndex, FOnChosen InOnChosen);

private:
	UFUNCTION()
	void HandleClicked();

	int32 SessionIndex = INDEX_NONE;
	FOnChosen OnChosen;
};

// Menu d'accueil : Solo contre l'IA, Héberger, liste des parties ouvertes (Steam, ou réseau local sans Steam),
// Rejoindre par adresse IP (sans Steam), Quitter.
// La mise en page est faite dans le Widget Blueprint WBP_MenuPrincipal (enfant de cette classe) ; le C++ relie
// ses boutons aux actions de UArenaGameInstance et remplit la liste des parties.
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
	// Appelée à chaque ajout à l'écran : on y affiche l'adresse mémorisée et le message laissé par une erreur réseau,
	// puis on lance la recherche des parties.
	virtual void NativeConstruct() override;
	// Appelée au retrait de l'écran (changement de carte) : on arrête d'écouter les recherches.
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SoloButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HostButton;

	// Titre de la liste des parties, et bouton pour relancer la recherche.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SessionsTitle;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RefreshButton;

	// Parties trouvées : un bouton par partie, ou une ligne d'état (recherche, aucune partie…).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> SessionList;

	// Ligne « adresse IP + Rejoindre », masquée avec Steam : un hôte Steam n'est joignable que par le relais de Steam.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> JoinRow;

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

	// Aide en bas du menu, écrite par le C++ selon le mode (Steam ou réseau local).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HelpText;

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Menu")
	FSlateColor InfoColor = FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f));

	UPROPERTY(EditDefaultsOnly, Category = "Arene|Menu")
	FSlateColor ErrorColor = FSlateColor(FLinearColor(1.f, 0.45f, 0.35f));

	// Tant que le menu est affiché, la liste des parties est actualisée toutes les RefreshInterval secondes.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Menu", meta = (ClampMin = "2"))
	float RefreshInterval = 10.f;

private:
	UFUNCTION()
	void HandleSoloClicked();

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void HandleSessionChosen(int32 SessionIndex);
	void HandleMenuMessage(const FText& Message, bool bError);

	// Recherche automatique (minuterie), suspendue pendant une création de partie ou une connexion.
	void HandleRefreshTimer();

	// Met la liste à l'écran à jour d'après la dernière recherche. Les boutons ne sont recréés que si les parties
	// changent (un bouton remplacé entre l'appui et le relâchement de la souris perdrait le clic) ; un ping qui varie
	// ne change que le texte.
	void UpdateSessionList();

	// « Partie de Camille · attend un adversaire · 35 ms »
	static FText FormatSession(const FArenaFoundSession& Session);

	void ShowMessage(const FText& Message, bool bError);

	UArenaGameInstance* GetArenaGameInstance() const;
	UArenaSessionSubsystem* GetSessions() const;

	// Ce qui est affiché dans la liste : les parties, ou à défaut la ligne d'état.
	TArray<FArenaFoundSession> DisplayedSessions;
	FString DisplayedStatus;
	bool bListBuilt = false;

	// Texte de chaque bouton de partie, dans l'ordre de DisplayedSessions.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SessionLabels;

	// Création de partie ou connexion lancée depuis ce menu : plus de recherche automatique jusqu'à un éventuel échec.
	bool bBusy = false;

	FTimerHandle RefreshTimer;
	FDelegateHandle SearchUpdatedHandle;
	FDelegateHandle MenuMessageHandle;
};

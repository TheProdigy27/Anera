#include "ArenaMenuWidget.h"
#include "Anera.h"
#include "ArenaGameInstance.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "ArenaMenu"

void UArenaMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SoloButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleSoloClicked);
	HostButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleHostClicked);
	JoinButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleJoinClicked);
	QuitButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleQuitClicked);
	AddressBox->OnTextCommitted.AddDynamic(this, &UArenaMenuWidget::HandleAddressCommitted);
}

void UArenaMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UArenaGameInstance* GameInstance = GetArenaGameInstance();
	if (!GameInstance)
	{
		ShowMessage(LOCTEXT("NoGameInstance", "Configuration incomplète : la Game Instance du projet doit être ArenaGameInstance."), true);
		return;
	}
	AddressBox->SetText(FText::FromString(GameInstance->GetLastJoinAddress()));
	ShowMessage(GameInstance->TakeMenuMessage(), true);
}

void UArenaMenuWidget::HandleSoloClicked()
{
	if (UArenaGameInstance* GameInstance = GetArenaGameInstance())
	{
		ShowMessage(LOCTEXT("LoadingSolo", "Chargement de l'arène…"), false);
		GameInstance->StartSolo();
	}
}

void UArenaMenuWidget::HandleHostClicked()
{
	if (UArenaGameInstance* GameInstance = GetArenaGameInstance())
	{
		ShowMessage(LOCTEXT("LoadingHost", "Ouverture de la partie…"), false);
		GameInstance->HostGame();
	}
}

void UArenaMenuWidget::HandleJoinClicked()
{
	UArenaGameInstance* GameInstance = GetArenaGameInstance();
	if (!GameInstance)
	{
		return;
	}
	const FString Address = AddressBox->GetText().ToString();
	FText Error;
	if (!GameInstance->JoinGame(Address, Error))
	{
		ShowMessage(Error, true);
		return;
	}
	ShowMessage(FText::Format(LOCTEXT("Connecting", "Connexion à {0}… (abandon au bout d'une vingtaine de secondes si l'hôte ne répond pas)"),
		FText::FromString(Address.TrimStartAndEnd())), false);
}

void UArenaMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UArenaMenuWidget::HandleAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleJoinClicked();
	}
}

void UArenaMenuWidget::ShowMessage(const FText& Message, bool bError)
{
	MessageText->SetText(Message);
	MessageText->SetColorAndOpacity(bError ? ErrorColor : InfoColor);
}

UArenaGameInstance* UArenaMenuWidget::GetArenaGameInstance() const
{
	return GetGameInstance<UArenaGameInstance>();
}

#undef LOCTEXT_NAMESPACE

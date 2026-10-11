#include "ArenaMenuWidget.h"
#include "Anera.h"
#include "ArenaDeckWidget.h"
#include "ArenaGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "ArenaMenu"

void UArenaSessionButton::Setup(int32 InSessionIndex, FOnChosen InOnChosen)
{
	SessionIndex = InSessionIndex;
	OnChosen = InOnChosen;
	OnClicked.AddUniqueDynamic(this, &UArenaSessionButton::HandleClicked);
}

void UArenaSessionButton::HandleClicked()
{
	OnChosen.ExecuteIfBound(SessionIndex);
}

void UArenaMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SoloButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleSoloClicked);
	HostButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleHostClicked);
	RefreshButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleRefreshClicked);
	JoinButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleJoinClicked);
	DeckButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleDeckClicked);
	QuitButton->OnClicked.AddDynamic(this, &UArenaMenuWidget::HandleQuitClicked);
	DeckPanel->OnClosed.BindUObject(this, &UArenaMenuWidget::HandleDeckClosed);
	DeckPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddressBox->OnTextCommitted.AddDynamic(this, &UArenaMenuWidget::HandleAddressCommitted);
}

void UArenaMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UArenaGameInstance* GameInstance = GetArenaGameInstance();
	UArenaSessionSubsystem* Sessions = GetSessions();
	if (!GameInstance || !Sessions)
	{
		ShowMessage(LOCTEXT("NoGameInstance", "Configuration incomplète : la Game Instance du projet doit être ArenaGameInstance."), true);
		return;
	}
	AddressBox->SetText(FText::FromString(GameInstance->GetLastJoinAddress()));
	ShowMessage(GameInstance->TakeMenuMessage(), true);
	bBusy = false;

	const bool bSteam = Sessions->IsUsingSteam();
	JoinRow->SetVisibility(bSteam ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	HelpText->SetText(bSteam
		? LOCTEXT("SteamHelp", "Héberger : votre partie apparaît dans la liste des autres joueurs, sans réglage de box.\n"
			"Rejoindre : cliquez sur une partie de la liste, ou acceptez une invitation Steam (ou « Rejoindre la partie » "
			"dans la liste d'amis Steam) pendant que le jeu est lancé.\nEn partie, Échap ramène au menu.")
		: LOCTEXT("LanHelp", "Steam n'est pas disponible (jeu lancé depuis l'éditeur, ou Steam fermé) : seules les parties "
			"du réseau local sont listées, et on peut aussi entrer l'adresse IP de l'hôte. Pour jouer par Internet sans Steam, "
			"l'hôte redirige le port 7777 (UDP) de sa box vers son PC et donne son adresse IP publique.\n"
			"En partie, Échap ramène au menu."));

	MenuMessageHandle = GameInstance->OnMenuMessage.AddUObject(this, &UArenaMenuWidget::HandleMenuMessage);
	SearchUpdatedHandle = Sessions->OnSearchUpdated.AddUObject(this, &UArenaMenuWidget::UpdateSessionList);
	bListBuilt = false;
	UpdateSessionList();

	// Une session restée ouverte (retour au menu par un chemin imprévu) est d'abord fermée, puis on cherche.
	Sessions->LeaveSession([WeakThis = TWeakObjectPtr<UArenaMenuWidget>(this)]()
	{
		UArenaSessionSubsystem* MenuSessions = WeakThis.IsValid() ? WeakThis->GetSessions() : nullptr;
		if (MenuSessions)
		{
			MenuSessions->FindSessions();
		}
	});
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UArenaMenuWidget::HandleRefreshTimer, RefreshInterval, true);
}

void UArenaMenuWidget::NativeDestruct()
{
	if (UArenaGameInstance* GameInstance = GetArenaGameInstance())
	{
		GameInstance->OnMenuMessage.Remove(MenuMessageHandle);
	}
	if (UArenaSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnSearchUpdated.Remove(SearchUpdatedHandle);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
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
	UArenaGameInstance* GameInstance = GetArenaGameInstance();
	UArenaSessionSubsystem* Sessions = GetSessions();
	if (GameInstance && Sessions)
	{
		bBusy = true;
		ShowMessage(Sessions->IsUsingSteam() ? LOCTEXT("CreatingSteam", "Création de la partie sur Steam…") : LOCTEXT("LoadingHost", "Ouverture de la partie…"), false);
		GameInstance->HostGame();
	}
}

void UArenaMenuWidget::HandleRefreshClicked()
{
	if (UArenaSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->FindSessions();
	}
}

void UArenaMenuWidget::HandleRefreshTimer()
{
	UArenaSessionSubsystem* Sessions = GetSessions();
	if (Sessions && !bBusy)
	{
		Sessions->FindSessions();
	}
}

void UArenaMenuWidget::HandleSessionChosen(int32 SessionIndex)
{
	UArenaGameInstance* GameInstance = GetArenaGameInstance();
	UArenaSessionSubsystem* Sessions = GetSessions();
	if (!GameInstance || !Sessions || !Sessions->GetFoundSessions().IsValidIndex(SessionIndex))
	{
		return;
	}
	bBusy = true;
	ShowMessage(FText::Format(LOCTEXT("JoiningSession", "Connexion à la partie de {0}…"),
		FText::FromString(Sessions->GetFoundSessions()[SessionIndex].HostName)), false);
	GameInstance->JoinFoundSession(SessionIndex);
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
	bBusy = true;
	ShowMessage(FText::Format(LOCTEXT("Connecting", "Connexion à {0}… (abandon au bout d'une vingtaine de secondes si l'hôte ne répond pas)"),
		FText::FromString(Address.TrimStartAndEnd())), false);
}

void UArenaMenuWidget::HandleDeckClicked()
{
	MainPanel->SetVisibility(ESlateVisibility::Collapsed);
	// Visible avant Open : l'écran du deck prend alors le focus clavier (pour Échap).
	DeckPanel->SetVisibility(ESlateVisibility::Visible);
	DeckPanel->Open();
}

void UArenaMenuWidget::HandleDeckClosed(bool bSaved)
{
	DeckPanel->SetVisibility(ESlateVisibility::Collapsed);
	MainPanel->SetVisibility(ESlateVisibility::Visible);
	// Le bouton « Deck » qui avait le focus est de nouveau là : le clavier revient au menu.
	DeckButton->SetFocus();
	if (bSaved)
	{
		ShowMessage(LOCTEXT("DeckSaved", "Deck enregistré : il servira à vos prochaines parties."), false);
	}
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

void UArenaMenuWidget::HandleMenuMessage(const FText& Message, bool bError)
{
	// Une erreur met fin à la création ou à la connexion lancée : la recherche automatique reprend.
	if (bError)
	{
		bBusy = false;
	}
	ShowMessage(Message, bError);
}

void UArenaMenuWidget::UpdateSessionList()
{
	const UArenaSessionSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		return;
	}
	const bool bSteam = Sessions->IsUsingSteam();
	const FText Title = bSteam ? LOCTEXT("SteamSessionsTitle", "Parties Steam ouvertes") : LOCTEXT("LanSessionsTitle", "Parties sur le réseau local");
	SessionsTitle->SetText(Sessions->IsSearching() ? FText::Format(LOCTEXT("TitleSearching", "{0} · recherche…"), Title) : Title);

	const TArray<FArenaFoundSession>& Found = Sessions->GetFoundSessions();
	FText Status;
	if (Found.IsEmpty())
	{
		if (Sessions->IsSearching())
		{
			Status = LOCTEXT("Searching", "Recherche des parties…");
		}
		else if (!Sessions->HasSearchSucceeded())
		{
			Status = LOCTEXT("SearchFailed", "La recherche a échoué. Réessayez avec « Actualiser ».");
		}
		else
		{
			Status = bSteam
				? LOCTEXT("NoSteamSession", "Aucune partie ouverte pour l'instant. Quand votre adversaire clique sur « Héberger une partie », elle apparaît ici.")
				: LOCTEXT("NoLanSession", "Aucune partie sur le réseau local.");
		}
	}
	// Mêmes parties qu'à l'écran, au ping près (il varie à chaque recherche) : on met à jour les textes sans recréer les boutons.
	bool bSameSessions = bListBuilt && Status.ToString() == DisplayedStatus && Found.Num() == DisplayedSessions.Num() && Found.Num() == SessionLabels.Num();
	for (int32 Index = 0; bSameSessions && Index < Found.Num(); ++Index)
	{
		bSameSessions = Found[Index].HostName == DisplayedSessions[Index].HostName && Found[Index].bInProgress == DisplayedSessions[Index].bInProgress;
	}
	if (bSameSessions)
	{
		for (int32 Index = 0; Index < Found.Num(); ++Index)
		{
			if (SessionLabels[Index])
			{
				SessionLabels[Index]->SetText(FormatSession(Found[Index]));
			}
		}
		DisplayedSessions = Found;
		return;
	}
	bListBuilt = true;
	DisplayedSessions = Found;
	DisplayedStatus = Status.ToString();
	SessionList->ClearChildren();
	SessionLabels.Reset();

	if (!Status.IsEmpty())
	{
		UTextBlock* StatusLine = WidgetTree->ConstructWidget<UTextBlock>();
		StatusLine->SetText(Status);
		FSlateFontInfo Font = MessageText->GetFont();
		Font.Size = 14;
		StatusLine->SetFont(Font);
		StatusLine->SetColorAndOpacity(InfoColor);
		StatusLine->SetAutoWrapText(true);
		SessionList->AddChild(StatusLine);
		return;
	}

	// Les boutons des parties reprennent le style du bouton « Solo » et de son texte, réglés dans WBP_MenuPrincipal.
	const UTextBlock* LabelTemplate = Cast<UTextBlock>(SoloButton->GetContent());
	for (int32 Index = 0; Index < Found.Num(); ++Index)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FormatSession(Found[Index]));
		SessionLabels.Add(Label);
		if (LabelTemplate)
		{
			FSlateFontInfo Font = LabelTemplate->GetFont();
			Font.Size = 16;
			Label->SetFont(Font);
			Label->SetColorAndOpacity(LabelTemplate->GetColorAndOpacity());
		}

		UArenaSessionButton* Button = WidgetTree->ConstructWidget<UArenaSessionButton>();
		Button->SetStyle(SoloButton->GetStyle());
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->AddChild(Label)))
		{
			LabelSlot->SetPadding(FMargin(14.f, 8.f));
			LabelSlot->SetHorizontalAlignment(HAlign_Left);
		}
		Button->Setup(Index, UArenaSessionButton::FOnChosen::CreateUObject(this, &UArenaMenuWidget::HandleSessionChosen));
		if (UScrollBoxSlot* ListSlot = Cast<UScrollBoxSlot>(SessionList->AddChild(Button)))
		{
			ListSlot->SetPadding(FMargin(0.f, 3.f));
		}
	}
}

FText UArenaMenuWidget::FormatSession(const FArenaFoundSession& Session)
{
	FText Text = Session.bInProgress
		? FText::Format(LOCTEXT("SessionInProgress", "Partie de {0} · en cours, une place libre"), FText::FromString(Session.HostName))
		: FText::Format(LOCTEXT("SessionWaiting", "Partie de {0} · attend un adversaire"), FText::FromString(Session.HostName));
	if (Session.PingMs >= 0)
	{
		Text = FText::Format(LOCTEXT("SessionPing", "{0} · {1} ms"), Text, FText::AsNumber(Session.PingMs));
	}
	return Text;
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

UArenaSessionSubsystem* UArenaMenuWidget::GetSessions() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

#undef LOCTEXT_NAMESPACE

#include "ArenaDeckWidget.h"
#include "ArenaCardData.h"
#include "ArenaCardWidget.h"
#include "ArenaDeck.h"
#include "ArenaGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

#define LOCTEXT_NAMESPACE "ArenaDeck"

void UArenaDeckWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SaveButton->OnClicked.AddDynamic(this, &UArenaDeckWidget::HandleSaveClicked);
	BackButton->OnClicked.AddDynamic(this, &UArenaDeckWidget::HandleBackClicked);
	// L'écran prend le focus clavier à l'ouverture, pour recevoir Échap.
	SetIsFocusable(true);
}

void UArenaDeckWidget::Open()
{
	const UArenaGameInstance* GameInstance = GetGameInstance<UArenaGameInstance>();
	Selection = GameInstance ? GameInstance->GetDeck() : TArray<UArenaCardData*>();
	// Les cartes du jeu ne changent pas en cours de route : la grille n'est construite qu'une fois.
	if (CardWidgets.IsEmpty())
	{
		BuildCardGrid();
	}
	ShowMessage(FText::GetEmpty(), false);
	Refresh();
	SetFocus();
}

void UArenaDeckWidget::BuildCardGrid()
{
	CardGrid->ClearChildren();
	CardWidgets.Reset();
	if (!CardWidgetClass)
	{
		ShowMessage(LOCTEXT("NoCardWidget", "Configuration incomplète : CardWidgetClass n'est pas réglée dans WBP_Deck."), true);
		return;
	}
	for (UArenaCardData* Card : ArenaDeck::GetAvailableCards())
	{
		UArenaCardWidget* CardWidget = CreateWidget<UArenaCardWidget>(this, CardWidgetClass);
		if (!CardWidget)
		{
			continue;
		}
		CardWidget->SetHandIndex(INDEX_NONE);
		CardWidget->SetOnClicked(UArenaCardWidget::FOnCardClicked::CreateUObject(this, &UArenaDeckWidget::HandleCardClicked));
		CardWidget->SetCard(Card);
		if (UWrapBoxSlot* CardSlot = CardGrid->AddChildToWrapBox(CardWidget))
		{
			// De la place au-dessus de chaque carte : une carte du deck est relevée.
			CardSlot->SetPadding(FMargin(6.f, 18.f, 6.f, 2.f));
		}
		CardWidgets.Add(CardWidget);
	}
	if (CardWidgets.IsEmpty())
	{
		ShowMessage(LOCTEXT("NoCards", "Aucune carte trouvée dans le jeu."), true);
	}
}

void UArenaDeckWidget::Refresh()
{
	const bool bFull = Selection.Num() >= ArenaDeck::DeckSize;
	for (UArenaCardWidget* CardWidget : CardWidgets)
	{
		// Carte du deck : relevée, cadre doré. Deck complet : les autres cartes sont grisées.
		const bool bInDeck = Selection.Contains(CardWidget->GetCard());
		CardWidget->SetState(bInDeck, bInDeck || !bFull, false);
	}

	// Coût moyen à la française : une décimale, avec une virgule.
	const FString Average = FString::Printf(TEXT("%.1f"), ArenaDeck::GetAverageCost(ObjectPtrDecay(Selection))).Replace(TEXT("."), TEXT(","));
	CountText->SetText(FText::Format(LOCTEXT("Count", "{0} / {1} cartes · coût moyen {2}"),
		FText::AsNumber(Selection.Num()), FText::AsNumber(ArenaDeck::DeckSize), FText::FromString(Average)));
	SaveButton->SetIsEnabled(Selection.Num() == ArenaDeck::DeckSize);
}

void UArenaDeckWidget::HandleCardClicked(UArenaCardWidget* CardWidget)
{
	UArenaCardData* Card = CardWidget ? CardWidget->GetCard() : nullptr;
	if (!Card)
	{
		return;
	}
	if (Selection.Contains(Card))
	{
		Selection.Remove(Card);
		ShowMessage(FText::GetEmpty(), false);
	}
	else if (Selection.Num() >= ArenaDeck::DeckSize)
	{
		ShowMessage(LOCTEXT("DeckFull", "Le deck est complet : retirez d'abord une carte."), true);
		return;
	}
	else
	{
		Selection.Add(Card);
		ShowMessage(FText::GetEmpty(), false);
	}
	Refresh();
}

void UArenaDeckWidget::HandleSaveClicked()
{
	UArenaGameInstance* GameInstance = GetGameInstance<UArenaGameInstance>();
	if (!GameInstance || !GameInstance->SetDeck(ObjectPtrDecay(Selection)))
	{
		ShowMessage(LOCTEXT("SaveFailed", "Le deck n'a pas pu être enregistré."), true);
		return;
	}
	Close(true);
}

void UArenaDeckWidget::HandleBackClicked()
{
	Close(false);
}

FReply UArenaDeckWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Close(false);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UArenaDeckWidget::Close(bool bSaved)
{
	OnClosed.ExecuteIfBound(bSaved);
}

void UArenaDeckWidget::ShowMessage(const FText& Message, bool bError)
{
	MessageText->SetText(Message);
	MessageText->SetColorAndOpacity(bError ? ErrorColor : InfoColor);
}

#undef LOCTEXT_NAMESPACE

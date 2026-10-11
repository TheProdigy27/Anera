#include "ArenaCardWidget.h"
#include "ArenaCardData.h"
#include "ArenaPlayerController.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UArenaCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Pas de carte tant que le HUD n'en a pas donné : Hidden garde la place, la rangée de cartes ne bouge pas.
	SetVisibility(ESlateVisibility::Hidden);
	ApplyState();
}

void UArenaCardWidget::SetCard(UArenaCardData* InCard)
{
	if (InCard == Card)
	{
		return;
	}
	Card = InCard;
	if (!Card)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	// L'aperçu de la carte suivante laisse passer les clics (HitTestInvisible) ; une carte de la main les reçoit.
	SetVisibility(HandIndex == INDEX_NONE ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
	NameText->SetText(Card->DisplayName);
	CostText->SetText(FText::AsNumber(Card->ElixirCost));
	CardFace->SetBrushColor(Card->CardType == EArenaCardType::Spell ? SpellFaceColor : UnitFaceColor);
	if (Card->Icon.IsNull())
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		// Charge la texture en arrière-plan (référence douce) puis l'affiche.
		IconImage->SetBrushFromSoftTexture(Card->Icon);
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UArenaCardWidget::SetHandIndex(int32 InHandIndex)
{
	HandIndex = InHandIndex;
	if (KeyText)
	{
		KeyText->SetText(HandIndex == INDEX_NONE ? FText::GetEmpty() : FText::AsNumber(HandIndex + 1));
	}
	if (Card)
	{
		SetVisibility(HandIndex == INDEX_NONE ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
	}
}

void UArenaCardWidget::SetState(bool bInSelected, bool bInPlayable, bool bInPending)
{
	if (bInSelected == bSelected && bInPlayable == bPlayable && bInPending == bPending)
	{
		return;
	}
	bSelected = bInSelected;
	bPlayable = bInPlayable;
	bPending = bInPending;
	ApplyState();
}

void UArenaCardWidget::ApplyState()
{
	CardFrame->SetBrushColor(bSelected ? SelectedFrameColor : FrameColor);
	// Déplacement au rendu seulement : la mise en page de la rangée n'est pas recalculée.
	SetRenderTranslation(FVector2D(0.f, bSelected ? -SelectedLift : 0.f));
	SetColorAndOpacity(bPending ? PendingTint : bPlayable ? FLinearColor::White : UnplayableTint);
}

FReply UArenaCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (HandIndex != INDEX_NONE && Card && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Choisit la carte et commence le glisser-déposer : le PlayerController suit ensuite la souris et le
		// relâchement du bouton, sur le terrain comme sur l'interface.
		if (AArenaPlayerController* Controller = GetOwningPlayer<AArenaPlayerController>())
		{
			Controller->BeginCardDrag(HandIndex);
		}
		// Handled, sans capturer la souris : le clic s'arrête à l'interface, mais les mouvements de la souris qui
		// suivent arrivent au jeu dès qu'elle quitte la carte (le disque de prévisualisation la suit).
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

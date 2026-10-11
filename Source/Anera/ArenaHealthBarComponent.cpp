#include "ArenaHealthBarComponent.h"
#include "ArenaCombat.h"
#include "ArenaHealthBarWidget.h"
#include "ArenaHealthComponent.h"

UArenaHealthBarComponent::UArenaHealthBarComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	// Taille voulue par le widget lui-même (réglée dans le Widget Blueprint), centrée sur le composant.
	SetDrawAtDesiredSize(true);
	SetPivot(FVector2D(0.5f, 0.5f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	CanCharacterStepUpOn = ECB_No;
}

void UArenaHealthBarComponent::BeginPlay()
{
	// Le widget est créé ici par UWidgetComponent, avec la classe réglée par l'acteur.
	Super::BeginPlay();
	Health = GetOwner() ? GetOwner()->FindComponentByClass<UArenaHealthComponent>() : nullptr;
	if (Health)
	{
		Health->OnHealthChanged.AddDynamic(this, &UArenaHealthBarComponent::HandleHealthChanged);
	}
	Refresh();
}

void UArenaHealthBarComponent::HandleHealthChanged(UArenaHealthComponent* ChangedHealth, float Damage, AActor* DamageCauser)
{
	Refresh();
}

void UArenaHealthBarComponent::Refresh()
{
	UArenaHealthBarWidget* Bar = Cast<UArenaHealthBarWidget>(GetUserWidgetObject());
	if (!Bar || !Health)
	{
		return;
	}
	const bool bFull = Health->GetHealth() >= Health->GetMaxHealth();
	const bool bShow = !Health->IsDead() && !(bHideWhenFull && bFull);
	// La couche écran du moteur réaffiche son conteneur à chaque image : on cache le widget lui-même.
	const ESlateVisibility Visibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Bar->GetVisibility() != Visibility)
	{
		Bar->SetVisibility(Visibility);
	}
	Bar->SetHealth(Health->GetHealth(), Health->GetMaxHealth(), ArenaCombat::GetTeam(GetOwner()));
}

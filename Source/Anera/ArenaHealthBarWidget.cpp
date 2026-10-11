#include "ArenaHealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UArenaHealthBarWidget::SetHealth(float Health, float MaxHealth, EArenaTeam Team)
{
	if (Health != ShownHealth || MaxHealth != ShownMaxHealth)
	{
		HealthBar->SetPercent(MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f);
		if (HealthText)
		{
			// Arrondi au-dessus : un bâtiment à 0,4 PV est encore debout, il affiche 1.
			HealthText->SetText(FText::AsNumber(FMath::CeilToInt(FMath::Max(0.f, Health))));
		}
		ShownHealth = Health;
		ShownMaxHealth = MaxHealth;
	}
	if (!bTeamShown || Team != ShownTeam)
	{
		HealthBar->SetFillColorAndOpacity(Team == EArenaTeam::A ? TeamAColor : TeamJColor);
		ShownTeam = Team;
		bTeamShown = true;
	}
}

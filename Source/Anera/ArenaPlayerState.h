#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArenaTypes.h"
#include "ArenaPlayerState.generated.h"

// État d'un joueur, visible par tous les clients.
// Pour l'instant : son camp. Élixir, deck et main arrivent en phase 1.
UCLASS()
class ANERA_API AArenaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	EArenaTeam GetTeam() const { return Team; }

	// Serveur uniquement : le GameMode attribue le camp à la connexion.
	void SetTeam(EArenaTeam NewTeam);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Répliqué : le serveur fixe la valeur, Unreal la recopie chez les clients.
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Arene")
	EArenaTeam Team = EArenaTeam::None;
};

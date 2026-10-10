#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "ArenaGameInstance.generated.h"

class UNetDriver;

// Instance du jeu : un seul objet, créé au lancement et gardé d'une carte à l'autre, alors que le GameMode
// et le PlayerController sont recréés à chaque carte. Elle porte ce qui doit survivre au passage
// menu → arène → menu : lancer une partie (solo, héberger, rejoindre), revenir au menu, et le message
// d'erreur réseau que le menu affichera au retour (partie complète, hôte injoignable, hôte parti…).
//
// Config = Game : les UPROPERTY(Config) sont relues au lancement et enregistrées par SaveConfig()
// dans Saved/Config/<plateforme>/Game.ini (ici, la dernière adresse saisie dans « Rejoindre »).
UCLASS(Config = Game)
class ANERA_API UArenaGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UArenaGameInstance();

	virtual void Init() override;
	virtual void Shutdown() override;

	// Solo contre l'IA : l'arène sans réseau.
	UFUNCTION(BlueprintCallable, Category = "Arene|Reseau")
	void StartSolo();

	// Héberger : l'arène en « listen server », cette machine est à la fois le serveur et le joueur J.
	// Elle écoute sur le port 7777 (UDP) et attend un adversaire.
	UFUNCTION(BlueprintCallable, Category = "Arene|Reseau")
	void HostGame();

	// Rejoindre l'hôte à cette adresse : « 192.168.1.20 », « 192.168.1.20:7777 » ou un nom de machine.
	// Faux, avec OutError, si l'adresse n'est pas valable. Sinon la connexion part à l'image suivante ;
	// en cas d'échec, le moteur revient au menu, qui affiche la raison.
	UFUNCTION(BlueprintCallable, Category = "Arene|Reseau")
	bool JoinGame(const FString& Address, FText& OutError);

	// Quitte la partie, ou la connexion en cours, et revient au menu. Si cette machine héberge, l'adversaire est déconnecté.
	UFUNCTION(BlueprintCallable, Category = "Arene|Reseau")
	void ReturnToMenu();

	// Message laissé pour le menu (erreur réseau), vide s'il n'y en a pas. Il est effacé une fois lu.
	UFUNCTION(BlueprintCallable, Category = "Arene|Reseau")
	FText TakeMenuMessage();

	// Dernière adresse saisie dans « Rejoindre », gardée d'une session à l'autre.
	UFUNCTION(BlueprintPure, Category = "Arene|Reseau")
	FString GetLastJoinAddress() const { return LastJoinAddress; }

	// Adresses IPv4 de cette machine sur le réseau local : celles vers lesquelles la box doit rediriger le port.
	static TArray<FString> GetLocalAddresses();

	// Port sur lequel ce monde accepte les joueurs, ou 0 s'il n'héberge pas de partie.
	static int32 GetListenPort(UWorld* World);

protected:
	// Carte de l'arène, ouverte par Solo et Héberger. Le menu, lui, est la carte par défaut du jeu
	// (Project Settings > Maps & Modes > Game Default Map) : c'est là que le moteur revient après une erreur réseau.
	UPROPERTY(EditDefaultsOnly, Category = "Arene|Reseau")
	TSoftObjectPtr<UWorld> ArenaMap;

private:
	// Annule une connexion en cours puis ouvre la carte, avec ces options d'URL (« listen » pour héberger).
	void OpenMap(const FString& MapPath, const FString& Options);

	// Vrai si ce net driver est celui de notre monde ou de notre connexion en cours. En PIE, plusieurs instances
	// du jeu tournent dans le même processus et reçoivent toutes les événements réseau du moteur.
	bool IsOwnNetDriver(const UNetDriver* NetDriver) const;

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	// bOverride : remplace un message déjà présent. Un même échec déclenche souvent plusieurs événements :
	// on garde le plus précis (le refus envoyé par l'hôte plutôt que la coupure qui le suit).
	void SetMenuMessage(const FText& Message, bool bOverride);

	UPROPERTY(Config)
	FString LastJoinAddress;

	// Adresse de la connexion en cours, pour les messages d'erreur.
	FString PendingAddress;

	FText MenuMessage;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};

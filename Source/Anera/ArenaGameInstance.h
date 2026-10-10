#pragma once

#include "CoreMinimal.h"
#include "ArenaSessionSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "ArenaGameInstance.generated.h"

class UNetDriver;

// Instance du jeu : un seul objet, créé au lancement et gardé d'une carte à l'autre, alors que le GameMode
// et le PlayerController sont recréés à chaque carte. Elle porte ce qui doit survivre au passage
// menu → arène → menu : lancer une partie (solo, héberger, rejoindre), revenir au menu, et le message
// d'erreur réseau que le menu affichera au retour (partie complète, hôte injoignable, hôte parti…).
// L'annonce des parties en ligne (Steam ou réseau local) est confiée à UArenaSessionSubsystem.
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

	// Héberger : annonce la partie (lobby Steam, ou réseau local sans Steam), puis ouvre l'arène en « listen server » :
	// cette machine est à la fois le serveur et le joueur J. Sans Steam, elle écoute sur le port 7777 (UDP).
	// Si Steam ne peut pas créer la partie, on reste au menu avec le message.
	UFUNCTION(BlueprintCallable, Category = "Arene|Reseau")
	void HostGame();

	// Rejoindre la partie n° Index de la liste du menu (UArenaSessionSubsystem::GetFoundSessions).
	void JoinFoundSession(int32 Index);

	// Rejoindre l'hôte à cette adresse : « 192.168.1.20 », « 192.168.1.20:7777 » ou un nom de machine (sans Steam).
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

	// Message pour le menu déjà affiché : échec d'une action lancée depuis le menu, invitation acceptée…
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMenuMessage, const FText& /*Message*/, bool /*bError*/);
	FOnMenuMessage OnMenuMessage;

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

	// Se connecte à l'hôte : « IP:port », ou « steam.<identifiant>:port » donné par une session Steam.
	// HostLabel désigne l'hôte dans les messages d'erreur (« 192.168.1.20 », « la partie de Camille »).
	void TravelToHost(const FString& Url, const FString& HostLabel);

	// Suite d'une entrée dans une session : connexion à l'hôte, ou message d'erreur au menu.
	UArenaSessionSubsystem::FOnJoinDone MakeJoinDone(const FString& HostName);

	void HandleInviteAccepted(const FOnlineSessionSearchResult& Session, const FString& HostName);

	// Affiche le message dans le menu s'il est à l'écran ; sinon une erreur est gardée pour le prochain menu.
	void ShowMenuMessage(const FText& Message, bool bError);

	// Vrai si ce net driver est celui de notre monde ou de notre connexion en cours. En PIE, plusieurs instances
	// du jeu tournent dans le même processus et reçoivent toutes les événements réseau du moteur.
	bool IsOwnNetDriver(const UNetDriver* NetDriver) const;

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	// bOverride : remplace un message déjà présent. Un même échec déclenche souvent plusieurs événements :
	// on garde le plus précis (le refus envoyé par l'hôte plutôt que la coupure qui le suit).
	void SetMenuMessage(const FText& Message, bool bOverride);

	UArenaSessionSubsystem* GetSessions() const;

	UPROPERTY(Config)
	FString LastJoinAddress;

	// Hôte de la connexion en cours, pour les messages d'erreur.
	FString PendingHostLabel;

	// Création de la partie en cours : un second clic sur « Héberger » est ignoré.
	bool bHostPending = false;

	FText MenuMessage;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle InviteAcceptedHandle;
};

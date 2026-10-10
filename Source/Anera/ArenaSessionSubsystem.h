#pragma once

#include "CoreMinimal.h"
#include "ArenaTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ArenaSessionSubsystem.generated.h"

// Une partie trouvée par la recherche, telle que le menu l'affiche.
struct FArenaFoundSession
{
	FString HostName;
	// Partie déjà commencée : un joueur s'est déconnecté et sa place attend qu'on la reprenne.
	bool bInProgress = false;
	// Ping en millisecondes, ou -1 s'il n'est pas connu.
	int32 PingMs = -1;

	bool operator==(const FArenaFoundSession& Other) const
	{
		return HostName == Other.HostName && bInProgress == Other.bInProgress && PingMs == Other.PingMs;
	}
};

// Parties en ligne : créer celle de l'hôte, chercher celles des autres, en rejoindre une, la quitter.
//
// Passe par l'Online Subsystem (OSS) du moteur, une interface commune à plusieurs services en ligne :
// - Steam quand il est lancé et que le jeu tourne hors éditeur : la partie est un lobby Steam (App ID 480 de
//   test, voir DefaultEngine.ini), visible des autres joueurs Steam, et la connexion passe par le relais de Valve
//   (pilote réseau SteamSockets), sans port à ouvrir ;
// - sinon le sous-système « NULL » du moteur : parties annoncées sur le réseau local, connexion par IP. C'est le
//   cas en PIE, où Steam n'est jamais activé.
//
// Une session en ligne est l'annonce de la partie (qui l'héberge, places libres, réglages). Elle est distincte de
// la connexion réseau, ouverte ensuite par le voyage vers l'adresse que la session donne.
// Un UGameInstanceSubsystem est créé et détruit avec l'instance du jeu : il survit aux changements de carte, comme
// la session qu'il gère.
UCLASS()
class ANERA_API UArenaSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_TwoParams(FOnHostDone, bool /*bSuccess*/, const FText& /*Error*/);
	DECLARE_DELEGATE_ThreeParams(FOnJoinDone, bool /*bSuccess*/, const FString& /*ConnectUrl*/, const FText& /*Error*/);
	DECLARE_MULTICAST_DELEGATE(FOnSearchUpdated);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInviteAccepted, const FOnlineSessionSearchResult& /*Session*/, const FString& /*HostName*/);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Vrai si les parties passent par Steam, faux pour le réseau local.
	bool IsUsingSteam() const;

	// Vrai si cette machine héberge une partie annoncée (session créée et pas encore fermée).
	bool IsHostingSession() const;

	// Annonce la partie de cette machine (2 joueurs), puis appelle OnDone. Une session restée ouverte est d'abord fermée.
	void HostSession(FOnHostDone OnDone);

	// Hôte : publie l'état de la partie avec la session. Une partie terminée n'apparaît plus dans les recherches.
	void SetHostedMatchPhase(EArenaMatchPhase Phase);

	// Lance une recherche, sauf si une autre est en cours. OnSearchUpdated prévient au début et à la fin.
	void FindSessions();
	bool IsSearching() const { return bSearching; }
	// Faux tant qu'aucune recherche n'est terminée, et après une recherche en échec.
	bool HasSearchSucceeded() const { return bSearchSucceeded; }
	const TArray<FArenaFoundSession>& GetFoundSessions() const { return FoundSessions; }
	FOnSearchUpdated OnSearchUpdated;

	// Rejoint la partie n° Index de la dernière recherche. OnDone reçoit l'adresse à laquelle se connecter.
	void JoinFoundSession(int32 Index, FOnJoinDone OnDone);
	// Rejoint une partie donnée par le service (invitation Steam acceptée).
	void JoinSearchResult(const FOnlineSessionSearchResult& Result, FOnJoinDone OnDone);

	// Invitation Steam acceptée, ou « Rejoindre la partie » choisi dans la liste d'amis Steam, pendant que le jeu tourne.
	FOnInviteAccepted OnInviteAccepted;

	// Ferme la session de cette machine : l'hôte retire sa partie des recherches, le client libère sa place.
	// Then (facultatif) est appelée une fois la session fermée, ou tout de suite s'il n'y en avait pas.
	void LeaveSession(TFunction<void()> Then = nullptr);

	// Nom affiché d'un hôte trouvé par la recherche ou reçu par invitation.
	static FString GetHostName(const FOnlineSessionSearchResult& Result);

private:
	IOnlineSessionPtr GetSessionInterface() const;
	// Même interface, après y avoir branché nos délégués (création, recherche, entrée, invitation) si besoin.
	IOnlineSessionPtr GetBoundSessionInterface();
	int32 GetLocalUserNum() const;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId,
		const FOnlineSessionSearchResult& InviteResult);

	// Exécute les actions qui attendaient la fermeture de la session.
	void RunAfterLeave();

	// Interface de sessions à laquelle nos délégués sont branchés (une par instance du jeu en PIE).
	IOnlineSessionPtr BoundSessions;
	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle InviteAcceptedHandle;

	FOnHostDone PendingHostDone;
	FOnJoinDone PendingJoinDone;

	TSharedPtr<FOnlineSessionSearch> Search;
	// Résultats retenus de la dernière recherche (nos parties, pas terminées), dans l'ordre de FoundSessions.
	TArray<FOnlineSessionSearchResult> FoundResults;
	TArray<FArenaFoundSession> FoundSessions;
	bool bSearching = false;
	bool bSearchSucceeded = false;

	bool bLeaving = false;
	TArray<TFunction<void()>> AfterLeave;
};

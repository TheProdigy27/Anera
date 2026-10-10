#include "ArenaSessionSubsystem.h"
#include "Anera.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"

#define LOCTEXT_NAMESPACE "ArenaSessions"

namespace
{
	// Réglage publié avec chaque partie de ce jeu. L'App ID 480 est partagé par tous les projets qui testent Steam :
	// la recherche ne garde que les lobbies qui portent cette valeur.
	const FName SessionGameKey(TEXT("ANERA"));
	const FString SessionGameValue(TEXT("arene-1v1"));

	// Phase de la partie (EArenaMatchPhase), mise à jour par l'hôte.
	const FName SessionPhaseKey(TEXT("ANERA_PHASE"));

	constexpr int32 MaxPlayers = 2;
	// Steam renvoie au plus ce nombre de lobbies, déjà filtrés sur SessionGameKey.
	constexpr int32 MaxSearchResults = 50;

	FText GetNoOnlineText()
	{
		return LOCTEXT("NoOnline", "Le service en ligne n'est pas disponible.");
	}

	// Sous-système NULL : le nom de l'hôte est « <nom du PC>-<32 chiffres hexadécimaux> ». On ne garde que le nom du PC.
	FString TrimNullUserSuffix(const FString& Name)
	{
		int32 Dash = INDEX_NONE;
		if (!Name.FindLastChar(TEXT('-'), Dash) || Dash <= 0 || Name.Len() - Dash - 1 != 32)
		{
			return Name;
		}
		for (int32 Index = Dash + 1; Index < Name.Len(); ++Index)
		{
			if (!FChar::IsHexDigit(Name[Index]))
			{
				return Name;
			}
		}
		return Name.Left(Dash);
	}
}

void UArenaSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Branche tout de suite l'invitation Steam : elle peut arriver avant toute action dans le menu.
	GetBoundSessionInterface();
	UE_LOG(LogArene, Log, TEXT("Parties en ligne : %s"), IsUsingSteam() ? TEXT("Steam") : TEXT("reseau local (Steam absent)"));
}

void UArenaSessionSubsystem::Deinitialize()
{
	if (BoundSessions)
	{
		// Fermeture du jeu : on n'attend pas la réponse. Steam retire de toute façon du lobby un joueur dont le jeu s'arrête.
		if (BoundSessions->GetNamedSession(NAME_GameSession))
		{
			BoundSessions->DestroySession(NAME_GameSession);
		}
		BoundSessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
		BoundSessions->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
		BoundSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
		BoundSessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
		BoundSessions.Reset();
	}
	AfterLeave.Reset();
	Super::Deinitialize();
}

IOnlineSessionPtr UArenaSessionSubsystem::GetSessionInterface() const
{
	// Online::GetSessionInterface(World) : en PIE, chaque instance du jeu a son propre sous-système, retrouvé par son monde.
	return Online::GetSessionInterface(GetGameInstance()->GetWorld());
}

IOnlineSessionPtr UArenaSessionSubsystem::GetBoundSessionInterface()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions == BoundSessions)
	{
		return Sessions;
	}
	// Au lancement, le monde n'existe pas encore : l'interface trouvée alors peut différer de celle de la partie (en PIE).
	if (BoundSessions)
	{
		BoundSessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
		BoundSessions->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
		BoundSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
		BoundSessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}
	BoundSessions = Sessions;
	if (Sessions)
	{
		CreateCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(this, &UArenaSessionSubsystem::HandleCreateSessionComplete));
		FindCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
			FOnFindSessionsCompleteDelegate::CreateUObject(this, &UArenaSessionSubsystem::HandleFindSessionsComplete));
		JoinCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateUObject(this, &UArenaSessionSubsystem::HandleJoinSessionComplete));
		InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UArenaSessionSubsystem::HandleSessionUserInviteAccepted));
	}
	return Sessions;
}

int32 UArenaSessionSubsystem::GetLocalUserNum() const
{
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	return LocalPlayer ? LocalPlayer->GetControllerId() : 0;
}

bool UArenaSessionSubsystem::IsUsingSteam() const
{
	const IOnlineSubsystem* Online = Online::GetSubsystem(GetGameInstance()->GetWorld());
	return Online && Online->GetSubsystemName() == STEAM_SUBSYSTEM;
}

bool UArenaSessionSubsystem::IsHostingSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FNamedOnlineSession* Session = Sessions ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	return Session && Session->bHosting;
}

void UArenaSessionSubsystem::HostSession(FOnHostDone OnDone)
{
	LeaveSession([this, OnDone]()
	{
		const IOnlineSessionPtr Sessions = GetBoundSessionInterface();
		if (!Sessions)
		{
			OnDone.ExecuteIfBound(false, GetNoOnlineText());
			return;
		}
		const bool bSteam = IsUsingSteam();
		FOnlineSessionSettings Settings;
		Settings.NumPublicConnections = MaxPlayers;
		Settings.bIsLANMatch = !bSteam;
		// Avec Steam, la partie est un lobby : il donne l'identifiant Steam de l'hôte, auquel le client se connecte
		// par le relais, et la liste d'amis Steam propose « Rejoindre la partie ».
		Settings.bUsesPresence = bSteam;
		Settings.bUseLobbiesIfAvailable = bSteam;
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinViaPresence = true;
		Settings.bAllowInvites = true;
		// Un joueur déconnecté peut revenir en cours de partie (AArenaGameMode::Logout garde sa place).
		Settings.bAllowJoinInProgress = true;
		Settings.Set(SessionGameKey, SessionGameValue, EOnlineDataAdvertisementType::ViaOnlineService);
		Settings.Set(SessionPhaseKey, static_cast<int32>(EArenaMatchPhase::Waiting), EOnlineDataAdvertisementType::ViaOnlineService);

		UE_LOG(LogArene, Log, TEXT("Creation de la partie en ligne (%s)"), bSteam ? TEXT("lobby Steam") : TEXT("reseau local"));
		PendingHostDone = OnDone;
		// Selon le service, la réponse arrive pendant l'appel (réseau local) ou plus tard (Steam).
		if (!Sessions->CreateSession(GetLocalUserNum(), NAME_GameSession, Settings) && PendingHostDone.IsBound())
		{
			HandleCreateSessionComplete(NAME_GameSession, false);
		}
	});
}

void UArenaSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}
	UE_LOG(LogArene, Log, TEXT("Partie en ligne %s"), bWasSuccessful ? TEXT("creee") : TEXT("non creee"));
	const FOnHostDone Done = PendingHostDone;
	PendingHostDone.Unbind();
	const FText Error = IsUsingSteam()
		? LOCTEXT("SteamHostFailed", "Steam n'a pas pu créer la partie. Vérifiez que Steam est connecté, puis réessayez.")
		: LOCTEXT("LanHostFailed", "La partie n'a pas pu être annoncée sur le réseau local.");
	Done.ExecuteIfBound(bWasSuccessful, bWasSuccessful ? FText::GetEmpty() : Error);
}

void UArenaSessionSubsystem::SetHostedMatchPhase(EArenaMatchPhase Phase)
{
	const IOnlineSessionPtr Sessions = GetBoundSessionInterface();
	FNamedOnlineSession* Session = Sessions ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Session || !Session->bHosting)
	{
		return;
	}
	// Pour qui cherche une partie, les prolongations sont une partie en cours comme une autre.
	const int32 Published = static_cast<int32>(Phase == EArenaMatchPhase::Overtime ? EArenaMatchPhase::InProgress : Phase);
	int32 Current = INDEX_NONE;
	Session->SessionSettings.Get(SessionPhaseKey, Current);
	if (Current == Published)
	{
		return;
	}
	FOnlineSessionSettings Settings = Session->SessionSettings;
	Settings.Set(SessionPhaseKey, Published, EOnlineDataAdvertisementType::ViaOnlineService);
	Sessions->UpdateSession(NAME_GameSession, Settings, true);
}

void UArenaSessionSubsystem::FindSessions()
{
	if (bSearching)
	{
		return;
	}
	const IOnlineSessionPtr Sessions = GetBoundSessionInterface();
	if (!Sessions)
	{
		bSearchSucceeded = false;
		OnSearchUpdated.Broadcast();
		return;
	}
	const bool bSteam = IsUsingSteam();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = MaxSearchResults;
	Search->bIsLanQuery = !bSteam;
	if (bSteam)
	{
		// Chercher des lobbies Steam, et non des serveurs dédiés.
		Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	Search->QuerySettings.Set(SessionGameKey, SessionGameValue, EOnlineComparisonOp::Equals);

	bSearching = true;
	OnSearchUpdated.Broadcast();
	if (!Sessions->FindSessions(GetLocalUserNum(), Search.ToSharedRef()) && bSearching)
	{
		HandleFindSessionsComplete(false);
	}
}

void UArenaSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (!bSearching)
	{
		return;
	}
	bSearching = false;
	bSearchSucceeded = bWasSuccessful;
	FoundResults.Reset();
	FoundSessions.Reset();

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const bool bSteam = IsUsingSteam();
	const TArray<FOnlineSessionSearchResult> NoResults;
	for (const FOnlineSessionSearchResult& Result : bWasSuccessful && Search ? Search->SearchResults : NoResults)
	{
		// Le service filtre déjà sur SessionGameKey (Steam), mais pas toujours (réseau local) : on revérifie.
		FString Game;
		if (!Result.IsValid() || !Result.Session.SessionSettings.Get(SessionGameKey, Game) || Game != SessionGameValue)
		{
			continue;
		}
		int32 Phase = static_cast<int32>(EArenaMatchPhase::Waiting);
		Result.Session.SessionSettings.Get(SessionPhaseKey, Phase);
		// Une partie terminée refuse les arrivées, et une partie complète n'a pas de place.
		if (Phase == static_cast<int32>(EArenaMatchPhase::Ended) || Result.Session.NumOpenPublicConnections <= 0)
		{
			continue;
		}

		FArenaFoundSession& Found = FoundSessions.AddDefaulted_GetRef();
		Found.HostName = GetHostName(Result);
		if (!bSteam && Sessions)
		{
			// Réseau local : le nom de l'hôte est celui de son PC, son adresse le distingue (deux jeux sur le même PC).
			Found.HostName = TrimNullUserSuffix(Found.HostName);
			FString Address;
			if (Sessions->GetResolvedConnectString(Result, NAME_GamePort, Address) && !Address.IsEmpty())
			{
				Found.HostName = FString::Printf(TEXT("%s (%s)"), *Found.HostName, *Address);
			}
		}
		Found.bInProgress = Phase != static_cast<int32>(EArenaMatchPhase::Waiting);
		Found.PingMs = Result.PingInMs > 0 && Result.PingInMs < MAX_QUERY_PING ? Result.PingInMs : -1;
		FoundResults.Add(Result);
	}
	TArray<FString> HostNames;
	for (const FArenaFoundSession& Found : FoundSessions)
	{
		HostNames.Add(Found.HostName);
	}
	UE_LOG(LogArene, Log, TEXT("Recherche de parties %s : %d resultat(s), %d retenu(s) [%s]"), bWasSuccessful ? TEXT("terminee") : TEXT("en echec"),
		Search ? Search->SearchResults.Num() : 0, FoundSessions.Num(), *FString::Join(HostNames, TEXT(", ")));
	OnSearchUpdated.Broadcast();
}

void UArenaSessionSubsystem::JoinFoundSession(int32 Index, FOnJoinDone OnDone)
{
	if (!FoundResults.IsValidIndex(Index))
	{
		OnDone.ExecuteIfBound(false, FString(), LOCTEXT("SessionGone", "Cette partie n'est plus dans la liste. Actualisez la liste."));
		return;
	}
	JoinSearchResult(FoundResults[Index], OnDone);
}

void UArenaSessionSubsystem::JoinSearchResult(const FOnlineSessionSearchResult& Result, FOnJoinDone OnDone)
{
	// Le résultat est copié : une nouvelle recherche peut remplacer la liste avant que la session soit fermée.
	LeaveSession([this, Result, OnDone]()
	{
		const IOnlineSessionPtr Sessions = GetBoundSessionInterface();
		if (!Sessions)
		{
			OnDone.ExecuteIfBound(false, FString(), GetNoOnlineText());
			return;
		}
		UE_LOG(LogArene, Log, TEXT("Entree dans la partie de %s"), *GetHostName(Result));
		PendingJoinDone = OnDone;
		if (!Sessions->JoinSession(GetLocalUserNum(), NAME_GameSession, Result) && PendingJoinDone.IsBound())
		{
			HandleJoinSessionComplete(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
		}
	});
}

void UArenaSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession || !PendingJoinDone.IsBound())
	{
		return;
	}
	const FOnJoinDone Done = PendingJoinDone;
	PendingJoinDone.Unbind();
	UE_LOG(LogArene, Log, TEXT("Entree dans la partie : %s"), LexToString(Result));

	// L'adresse de l'hôte : « IP:port » sur le réseau local, « steam.<identifiant Steam>:port » avec Steam.
	FString Url;
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Result == EOnJoinSessionCompleteResult::Success && Sessions && Sessions->GetResolvedConnectString(NAME_GameSession, Url) && !Url.IsEmpty())
	{
		Done.ExecuteIfBound(true, Url, FText::GetEmpty());
		return;
	}

	FText Error;
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::SessionIsFull:
		Error = LOCTEXT("JoinFull", "La partie est complète (2 joueurs).");
		break;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		Error = LOCTEXT("JoinGone", "Cette partie n'existe plus : l'hôte l'a fermée. Actualisez la liste.");
		break;
	case EOnJoinSessionCompleteResult::Success:
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		Error = LOCTEXT("JoinNoAddress", "L'adresse de l'hôte est introuvable. Actualisez la liste et réessayez.");
		break;
	default:
		Error = LOCTEXT("JoinFailed", "Impossible d'entrer dans cette partie. Actualisez la liste et réessayez.");
		break;
	}
	// Sans adresse, la place prise dans la session ne sert à rien : on la rend.
	LeaveSession();
	Done.ExecuteIfBound(false, FString(), Error);
}

void UArenaSessionSubsystem::HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId,
	const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		UE_LOG(LogArene, Warning, TEXT("Invitation acceptee, mais la partie est introuvable"));
		return;
	}
	UE_LOG(LogArene, Log, TEXT("Invitation acceptee : partie de %s"), *GetHostName(InviteResult));
	OnInviteAccepted.Broadcast(InviteResult, GetHostName(InviteResult));
}

void UArenaSessionSubsystem::LeaveSession(TFunction<void()> Then)
{
	if (Then)
	{
		AfterLeave.Add(MoveTemp(Then));
	}
	if (bLeaving)
	{
		return;
	}
	const IOnlineSessionPtr Sessions = GetBoundSessionInterface();
	if (!Sessions || !Sessions->GetNamedSession(NAME_GameSession))
	{
		RunAfterLeave();
		return;
	}
	UE_LOG(LogArene, Log, TEXT("Fermeture de la session en ligne"));
	bLeaving = true;
	// Comme pour la création, la réponse peut arriver pendant l'appel.
	if (!Sessions->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateUObject(this, &UArenaSessionSubsystem::HandleDestroySessionComplete))
		&& bLeaving)
	{
		HandleDestroySessionComplete(NAME_GameSession, false);
	}
}

void UArenaSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bLeaving)
	{
		return;
	}
	UE_CLOG(!bWasSuccessful, LogArene, Warning, TEXT("La session en ligne n'a pas pu etre fermee proprement"));
	bLeaving = false;
	RunAfterLeave();
}

void UArenaSessionSubsystem::RunAfterLeave()
{
	// Une action peut en ajouter d'autres (LeaveSession dans une action) : on vide la liste avant de l'exécuter.
	TArray<TFunction<void()>> Actions = MoveTemp(AfterLeave);
	AfterLeave.Reset();
	for (TFunction<void()>& Action : Actions)
	{
		Action();
	}
}

FString UArenaSessionSubsystem::GetHostName(const FOnlineSessionSearchResult& Result)
{
	const FString Name = Result.Session.OwningUserName.TrimStartAndEnd();
	return Name.IsEmpty() ? FString(TEXT("?")) : Name;
}

#undef LOCTEXT_NAMESPACE

#include "ArenaGameInstance.h"
#include "Anera.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "ArenaReseau"

UArenaGameInstance::UArenaGameInstance()
{
	ArenaMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Arene/Maps/Arene.Arene")));
}

void UArenaGameInstance::Init()
{
	// Super::Init crée aussi les sous-systèmes de l'instance, dont UArenaSessionSubsystem.
	Super::Init();
	// Le moteur signale ici les connexions refusées, perdues ou impossibles, juste avant de revenir au menu.
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UArenaGameInstance::HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UArenaGameInstance::HandleTravelFailure);
	if (UArenaSessionSubsystem* Sessions = GetSessions())
	{
		InviteAcceptedHandle = Sessions->OnInviteAccepted.AddUObject(this, &UArenaGameInstance::HandleInviteAccepted);
	}
}

void UArenaGameInstance::Shutdown()
{
	GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	if (UArenaSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnInviteAccepted.Remove(InviteAcceptedHandle);
	}
	Super::Shutdown();
}

UArenaSessionSubsystem* UArenaGameInstance::GetSessions() const
{
	return GetSubsystem<UArenaSessionSubsystem>();
}

void UArenaGameInstance::StartSolo()
{
	MenuMessage = FText::GetEmpty();
	OpenMap(ArenaMap.GetLongPackageName(), FString());
}

void UArenaGameInstance::HostGame()
{
	UArenaSessionSubsystem* Sessions = GetSessions();
	if (bHostPending || !Sessions)
	{
		return;
	}
	MenuMessage = FText::GetEmpty();
	bHostPending = true;
	Sessions->HostSession(UArenaSessionSubsystem::FOnHostDone::CreateWeakLambda(this, [this](bool bSuccess, const FText& Error)
	{
		bHostPending = false;
		if (!bSuccess && GetSessions()->IsUsingSteam())
		{
			// Sans lobby, personne ne trouverait la partie : on reste au menu.
			ShowMenuMessage(Error, true);
			return;
		}
		// Réseau local : sans annonce, la partie reste joignable par son adresse IP.
		UE_CLOG(!bSuccess, LogArene, Warning, TEXT("%s La partie reste joignable par son adresse IP."), *Error.ToString());
		// « listen » : la carte s'ouvre avec un serveur réseau qui accepte les connexions (port 7777 par défaut).
		OpenMap(ArenaMap.GetLongPackageName(), TEXT("listen"));
	}));
}

void UArenaGameInstance::JoinFoundSession(int32 Index)
{
	UArenaSessionSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		return;
	}
	const TArray<FArenaFoundSession>& Found = Sessions->GetFoundSessions();
	const FString HostName = Found.IsValidIndex(Index) ? Found[Index].HostName : FString();
	Sessions->JoinFoundSession(Index, MakeJoinDone(HostName));
}

UArenaSessionSubsystem::FOnJoinDone UArenaGameInstance::MakeJoinDone(const FString& HostName)
{
	return UArenaSessionSubsystem::FOnJoinDone::CreateWeakLambda(this, [this, HostName](bool bSuccess, const FString& Url, const FText& Error)
	{
		if (!bSuccess)
		{
			ShowMenuMessage(Error, true);
			return;
		}
		TravelToHost(Url, FText::Format(LOCTEXT("HostGameLabel", "la partie de {0}"), FText::FromString(HostName)).ToString());
	});
}

void UArenaGameInstance::HandleInviteAccepted(const FOnlineSessionSearchResult& Session, const FString& HostName)
{
	// Le joueur a choisi cette partie dans Steam : on quitte la partie en cours, s'il y en a une, pour la rejoindre.
	ShowMenuMessage(FText::Format(LOCTEXT("InviteAccepted", "Invitation acceptée : connexion à la partie de {0}…"), FText::FromString(HostName)), false);
	GetSessions()->JoinSearchResult(Session, MakeJoinDone(HostName));
}

bool UArenaGameInstance::JoinGame(const FString& Address, FText& OutError)
{
	FString Host = Address.TrimStartAndEnd();
	if (Host.IsEmpty())
	{
		OutError = LOCTEXT("EmptyAddress", "Entrez l'adresse IP de l'hôte, par exemple 192.168.1.20.");
		return false;
	}
	// Le texte devient une URL Unreal : ses options (« ? ») ou un chemin de carte (« / ») n'ont rien à faire ici.
	if (Host.Contains(TEXT("?")) || Host.Contains(TEXT("#")) || Host.Contains(TEXT("/")) || Host.Contains(TEXT("\\")))
	{
		OutError = FText::Format(LOCTEXT("InvalidAddress", "Adresse invalide : {0}. Exemples : 192.168.1.20 ou 192.168.1.20:7777."), FText::FromString(Host));
		return false;
	}
	// Sans point ni port (« localhost », nom de machine), Unreal y verrait un nom de carte : on ajoute le port par défaut.
	if (!Host.Contains(TEXT(".")) && !Host.Contains(TEXT(":")))
	{
		Host += FString::Printf(TEXT(":%d"), FURL::UrlConfig.DefaultPort);
	}
	const FURL Url(nullptr, *Host, TRAVEL_Absolute);
	if (!Url.Valid || Url.Host.IsEmpty() || Url.Port <= 0 || Url.Port > 65535)
	{
		OutError = FText::Format(LOCTEXT("InvalidAddress", "Adresse invalide : {0}. Exemples : 192.168.1.20 ou 192.168.1.20:7777."), FText::FromString(Host));
		return false;
	}

	LastJoinAddress = Address.TrimStartAndEnd();
	SaveConfig();

	if (UArenaSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->LeaveSession();
	}
	TravelToHost(Host, Host);
	return true;
}

void UArenaGameInstance::TravelToHost(const FString& Url, const FString& HostLabel)
{
	if (UWorld* World = GetWorld())
	{
		GEngine->CancelPending(World);
	}
	MenuMessage = FText::GetEmpty();
	PendingHostLabel = HostLabel;
	UE_LOG(LogArene, Log, TEXT("Connexion a %s (%s)"), *HostLabel, *Url);
	// Le voyage a lieu à l'image suivante : le moteur se connecte, puis charge la carte de l'hôte.
	GEngine->SetClientTravel(GetWorld(), *Url, TRAVEL_Absolute);
}

void UArenaGameInstance::ReturnToMenu()
{
	// L'hôte retire sa partie des recherches ; le client rend sa place.
	if (UArenaSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->LeaveSession();
	}
	OpenMap(FPackageName::ObjectPathToPackageName(UGameMapsSettings::GetGameDefaultMap()), FString());
}

FText UArenaGameInstance::TakeMenuMessage()
{
	FText Message = MenuMessage;
	MenuMessage = FText::GetEmpty();
	return Message;
}

void UArenaGameInstance::ShowMenuMessage(const FText& Message, bool bError)
{
	if (OnMenuMessage.IsBound())
	{
		OnMenuMessage.Broadcast(Message, bError);
	}
	else if (bError)
	{
		SetMenuMessage(Message, true);
	}
}

void UArenaGameInstance::OpenMap(const FString& MapPath, const FString& Options)
{
	if (UWorld* World = GetWorld())
	{
		GEngine->CancelPending(World);
	}
	PendingHostLabel.Reset();
	UGameplayStatics::OpenLevel(this, FName(*MapPath), true, Options);
}

TArray<FString> UArenaGameInstance::GetLocalAddresses()
{
	TArray<FString> Result;
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (!Sockets || !Sockets->GetLocalAdapterAddresses(Addresses))
	{
		return Result;
	}
	for (const TSharedPtr<FInternetAddr>& Address : Addresses)
	{
		if (!Address || Address->GetProtocolType() != FNetworkProtocolTypes::IPv4)
		{
			continue;
		}
		// Ni la boucle locale (127.x), ni les adresses qu'une carte réseau se donne faute de box (169.254.x).
		const FString Text = Address->ToString(false);
		if (!Text.StartsWith(TEXT("127.")) && !Text.StartsWith(TEXT("169.254.")))
		{
			Result.AddUnique(Text);
		}
	}
	return Result;
}

int32 UArenaGameInstance::GetListenPort(UWorld* World)
{
	UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr;
	if (!NetDriver || NetDriver->ServerConnection)
	{
		// Pas de réseau, ou client d'une autre machine.
		return 0;
	}
	// Si le port 7777 est déjà pris (deux jeux sur le même PC), le moteur essaie les suivants.
	const TSharedPtr<const FInternetAddr> LocalAddress = NetDriver->GetLocalAddr();
	return LocalAddress ? LocalAddress->GetPort() : World->URL.Port;
}

bool UArenaGameInstance::IsOwnNetDriver(const UNetDriver* NetDriver) const
{
	const FWorldContext* Context = GetWorldContext();
	if (!NetDriver || !Context)
	{
		return false;
	}
	if (Context->PendingNetGame && Context->PendingNetGame->NetDriver == NetDriver)
	{
		return true;
	}
	return NetDriver->GetWorld() && NetDriver->GetWorld() == GetWorld();
}

void UArenaGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!IsOwnNetDriver(NetDriver))
	{
		return;
	}
	const bool bConnecting = NetDriver->NetDriverName == NAME_PendingNetDriver;
	UE_LOG(LogArene, Log, TEXT("Echec reseau (%s%s) : %s"), ENetworkFailure::ToString(FailureType),
		bConnecting ? TEXT(", connexion en cours") : TEXT(""), *ErrorString);

	// Côté hôte, un client qui part ne change rien ici : la partie continue et sa place reste libre.
	if (!NetDriver->ServerConnection)
	{
		return;
	}

	// Le client quitte aussi la session en ligne, pour libérer sa place (et pouvoir revenir).
	UArenaSessionSubsystem* Sessions = GetSessions();
	const bool bSteam = Sessions && Sessions->IsUsingSteam();
	if (Sessions)
	{
		Sessions->LeaveSession();
	}

	const FText Address = FText::FromString(PendingHostLabel.IsEmpty() ? TEXT("l'hôte") : PendingHostLabel);
	// Textes du moteur : connexion fermée sans raison donnée, et hôte qui quitte. Le second est traduit dans
	// la langue de l'hôte avant l'envoi : on le compare aussi au texte anglais d'origine.
	const FString EngineConnectionLost = NSLOCTEXT("Engine", "ConnectionFailed", "Your connection to the host has been lost.").ToString();
	const FText EngineHostClosed = NSLOCTEXT("NetworkErrors", "HostClosedConnection", "Host closed the connection.");
	const bool bHostClosed = ErrorString == EngineHostClosed.ToString() || ErrorString == TEXT("Host closed the connection.");

	switch (FailureType)
	{
	case ENetworkFailure::FailureReceived:
	case ENetworkFailure::PendingConnectionFailure:
		if (bHostClosed)
		{
			SetMenuMessage(LOCTEXT("HostClosed", "L'hôte a quitté la partie."), true);
			break;
		}
		if (!ErrorString.IsEmpty() && ErrorString != EngineConnectionLost)
		{
			// Raison envoyée par l'hôte (« La partie est complète »…) : c'est la plus utile.
			SetMenuMessage(FText::FromString(ErrorString), true);
			break;
		}
		if (!bConnecting)
		{
			SetMenuMessage(LOCTEXT("ConnectionLost", "La connexion avec l'hôte a été coupée, ou il a quitté la partie."), false);
		}
		else if (bSteam)
		{
			SetMenuMessage(FText::Format(LOCTEXT("SteamConnectFailed", "Impossible de joindre {0} par Steam. Actualisez la liste et réessayez."), Address), false);
		}
		else
		{
			SetMenuMessage(FText::Format(LOCTEXT("ConnectFailed", "Impossible de joindre {0}. Vérifiez l'adresse, et que l'hôte attend bien un adversaire."), Address), false);
		}
		break;
	case ENetworkFailure::ConnectionTimeout:
		if (!bConnecting)
		{
			SetMenuMessage(LOCTEXT("TimedOut", "L'hôte ne répond plus : la connexion a été perdue."), false);
		}
		else if (bSteam)
		{
			SetMenuMessage(FText::Format(LOCTEXT("SteamNoAnswer", "Aucune réponse de {0} par Steam. L'hôte a peut-être quitté : actualisez la liste et réessayez."), Address), false);
		}
		else
		{
			SetMenuMessage(FText::Format(LOCTEXT("NoAnswer", "Aucune réponse de {0}. Vérifiez l'adresse, que l'hôte attend bien un adversaire, et que le port 7777 (UDP) de sa box est redirigé vers son PC."), Address), false);
		}
		break;
	case ENetworkFailure::ConnectionLost:
		SetMenuMessage(LOCTEXT("ConnectionLost", "La connexion avec l'hôte a été coupée, ou il a quitté la partie."), false);
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		SetMenuMessage(LOCTEXT("Outdated", "L'hôte et vous n'avez pas la même version du jeu."), true);
		break;
	default:
		SetMenuMessage(FText::Format(LOCTEXT("NetworkError", "Erreur réseau : {0}"), FText::FromString(ErrorString)), false);
		break;
	}
}

void UArenaGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World != GetWorld())
	{
		return;
	}
	UE_LOG(LogArene, Log, TEXT("Echec du voyage (%s) : %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	if (UArenaSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->LeaveSession();
	}
	const FText Address = FText::FromString(PendingHostLabel.IsEmpty() ? TEXT("l'hôte") : PendingHostLabel);
	if (FailureType == ETravelFailure::PendingNetGameCreateFailure)
	{
		// Le pilote réseau n'a pas pu ouvrir la connexion (adresse Steam injoignable, réseau coupé…) : le texte du moteur est en anglais.
		SetMenuMessage(FText::Format(LOCTEXT("ConnectionNotStarted", "Impossible d'ouvrir la connexion vers {0}. Actualisez la liste et réessayez."), Address), false);
		return;
	}
	SetMenuMessage(FText::Format(LOCTEXT("TravelFailed", "Impossible de rejoindre {0} : {1}"), Address, FText::FromString(ErrorString)), false);
}

void UArenaGameInstance::SetMenuMessage(const FText& Message, bool bOverride)
{
	if (bOverride || MenuMessage.IsEmpty())
	{
		MenuMessage = Message;
	}
}

namespace
{
	// Commande de test : « Arene.Heberger » fait comme le bouton « Héberger une partie » du menu. Utile hors éditeur,
	// où MCP ne peut pas cliquer : UnrealEditor.exe Anera.uproject -game -ExecCmds="Arene.Heberger".
	void HostFromConsole(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		UArenaGameInstance* GameInstance = ConsoleWorld ? ConsoleWorld->GetGameInstance<UArenaGameInstance>() : nullptr;
		if (!GameInstance)
		{
			UE_LOG(LogArene, Warning, TEXT("Arene.Heberger : pas d'instance du jeu (lancer le jeu d'abord)."));
			return;
		}
		UE_LOG(LogArene, Log, TEXT("Arene.Heberger"));
		GameInstance->HostGame();
	}

	FAutoConsoleCommandWithWorldAndArgs HostCommand(
		TEXT("Arene.Heberger"),
		TEXT("Arene.Heberger : héberge une partie, comme le bouton du menu (test sans souris)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HostFromConsole));

	// Commande de test : « Arene.RejoindrePartie [n] » rejoint la partie n (1 par défaut) de la liste du menu.
	// Si elle n'y est pas encore, la commande attend les recherches suivantes (le menu en lance une toutes les 10 s) :
	// elle peut donc être passée au lancement, avec -ExecCmds.
	void JoinFromConsole(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		UArenaGameInstance* GameInstance = ConsoleWorld ? ConsoleWorld->GetGameInstance<UArenaGameInstance>() : nullptr;
		UArenaSessionSubsystem* Sessions = GameInstance ? GameInstance->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
		if (!Sessions)
		{
			UE_LOG(LogArene, Warning, TEXT("Arene.RejoindrePartie : pas d'instance du jeu (lancer le jeu d'abord)."));
			return;
		}
		const int32 Index = FMath::Max(0, (Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1) - 1);
		if (Sessions->GetFoundSessions().IsValidIndex(Index))
		{
			UE_LOG(LogArene, Log, TEXT("Arene.RejoindrePartie : partie %d"), Index + 1);
			GameInstance->JoinFoundSession(Index);
			return;
		}
		UE_LOG(LogArene, Log, TEXT("Arene.RejoindrePartie : en attente de la partie %d dans les recherches"), Index + 1);
		static FDelegateHandle WaitHandle;
		Sessions->OnSearchUpdated.Remove(WaitHandle);
		WaitHandle = Sessions->OnSearchUpdated.AddLambda([WeakGameInstance = TWeakObjectPtr<UArenaGameInstance>(GameInstance), Index]()
		{
			UArenaGameInstance* WaitingGameInstance = WeakGameInstance.Get();
			UArenaSessionSubsystem* WaitingSessions = WaitingGameInstance ? WaitingGameInstance->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
			if (WaitingSessions && !WaitingSessions->IsSearching() && WaitingSessions->GetFoundSessions().IsValidIndex(Index))
			{
				WaitingSessions->OnSearchUpdated.Remove(WaitHandle);
				UE_LOG(LogArene, Log, TEXT("Arene.RejoindrePartie : partie %d trouvee"), Index + 1);
				WaitingGameInstance->JoinFoundSession(Index);
			}
		});
	}

	FAutoConsoleCommandWithWorldAndArgs JoinCommand(
		TEXT("Arene.RejoindrePartie"),
		TEXT("Arene.RejoindrePartie [n] : rejoint la partie n (1 par défaut) de la liste du menu, en attendant qu'elle y apparaisse (test sans souris)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JoinFromConsole));
}

#undef LOCTEXT_NAMESPACE

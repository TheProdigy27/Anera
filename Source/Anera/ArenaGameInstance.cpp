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
	Super::Init();
	// Le moteur signale ici les connexions refusées, perdues ou impossibles, juste avant de revenir au menu.
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UArenaGameInstance::HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UArenaGameInstance::HandleTravelFailure);
}

void UArenaGameInstance::Shutdown()
{
	GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	Super::Shutdown();
}

void UArenaGameInstance::StartSolo()
{
	MenuMessage = FText::GetEmpty();
	OpenMap(ArenaMap.GetLongPackageName(), FString());
}

void UArenaGameInstance::HostGame()
{
	MenuMessage = FText::GetEmpty();
	// « listen » : la carte s'ouvre avec un serveur réseau qui accepte les connexions (port 7777 par défaut).
	OpenMap(ArenaMap.GetLongPackageName(), TEXT("listen"));
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

	if (UWorld* World = GetWorld())
	{
		GEngine->CancelPending(World);
	}
	MenuMessage = FText::GetEmpty();
	PendingAddress = Host;
	UE_LOG(LogArene, Log, TEXT("Connexion a %s"), *Host);
	// Le voyage a lieu à l'image suivante : le moteur se connecte, puis charge la carte de l'hôte.
	GEngine->SetClientTravel(GetWorld(), *Host, TRAVEL_Absolute);
	return true;
}

void UArenaGameInstance::ReturnToMenu()
{
	OpenMap(FPackageName::ObjectPathToPackageName(UGameMapsSettings::GetGameDefaultMap()), FString());
}

FText UArenaGameInstance::TakeMenuMessage()
{
	FText Message = MenuMessage;
	MenuMessage = FText::GetEmpty();
	return Message;
}

void UArenaGameInstance::OpenMap(const FString& MapPath, const FString& Options)
{
	if (UWorld* World = GetWorld())
	{
		GEngine->CancelPending(World);
	}
	PendingAddress.Reset();
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

	const FText Address = FText::FromString(PendingAddress.IsEmpty() ? TEXT("l'hôte") : PendingAddress);
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
		SetMenuMessage(bConnecting
			? FText::Format(LOCTEXT("ConnectFailed", "Impossible de joindre {0}. Vérifiez l'adresse, et que l'hôte attend bien un adversaire."), Address)
			: LOCTEXT("ConnectionLost", "La connexion avec l'hôte a été coupée, ou il a quitté la partie."), false);
		break;
	case ENetworkFailure::ConnectionTimeout:
		SetMenuMessage(bConnecting
			? FText::Format(LOCTEXT("NoAnswer", "Aucune réponse de {0}. Vérifiez l'adresse, que l'hôte attend bien un adversaire, et que le port 7777 (UDP) de sa box est redirigé vers son PC."), Address)
			: LOCTEXT("TimedOut", "L'hôte ne répond plus : la connexion a été perdue."), false);
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
	const FText Address = FText::FromString(PendingAddress.IsEmpty() ? TEXT("l'hôte") : PendingAddress);
	SetMenuMessage(FText::Format(LOCTEXT("TravelFailed", "Impossible de rejoindre {0} : {1}"), Address, FText::FromString(ErrorString)), false);
}

void UArenaGameInstance::SetMenuMessage(const FText& Message, bool bOverride)
{
	if (bOverride || MenuMessage.IsEmpty())
	{
		MenuMessage = Message;
	}
}

#undef LOCTEXT_NAMESPACE

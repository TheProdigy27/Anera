using UnrealBuildTool;

// Module principal du jeu : déclare les modules du moteur dont il dépend.
public class Anera : ModuleRules
{
	public Anera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "GameplayTasks", "NavigationSystem", "UMG", "Slate", "SlateCore" });

		// OnlineSubsystem : parties en ligne (sessions Steam, ou réseau local sans Steam). Le service utilisé est choisi
		// dans DefaultEngine.ini ([OnlineSubsystem] DefaultPlatformService), le code du jeu ne dépend pas de Steam.
		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "EngineSettings", "OnlineSubsystem", "OnlineSubsystemUtils" });
	}
}

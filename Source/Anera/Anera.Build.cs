using UnrealBuildTool;

// Module principal du jeu : déclare les modules du moteur dont il dépend.
public class Anera : ModuleRules
{
	public Anera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "GameplayTasks", "NavigationSystem", "UMG", "Slate", "SlateCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "EngineSettings" });
	}
}

using UnrealBuildTool;

// Module principal du jeu : déclare les modules du moteur dont il dépend.
public class Anera : ModuleRules
{
	public Anera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}

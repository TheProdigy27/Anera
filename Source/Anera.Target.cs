using UnrealBuildTool;
using System.Collections.Generic;

// Cible « jeu » : l'exécutable packagé (sans l'éditeur).
public class AneraTarget : TargetRules
{
	public AneraTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Anera");
	}
}

using UnrealBuildTool;
using System.Collections.Generic;

// Cible « éditeur » : le module du jeu chargé dans l'Unreal Editor.
public class AneraEditorTarget : TargetRules
{
	public AneraEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Anera");
	}
}

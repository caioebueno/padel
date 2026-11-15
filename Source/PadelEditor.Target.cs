using UnrealBuildTool;

public class PadelEditorTarget : TargetRules
{
	public PadelEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "Padel", "PadelCore" });
	}
}

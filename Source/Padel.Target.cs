using UnrealBuildTool;

public class PadelTarget : TargetRules
{
	public PadelTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "Padel", "PadelCore" });
	}
}

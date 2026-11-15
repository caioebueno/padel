using UnrealBuildTool;

public class Padel : ModuleRules
{
	public Padel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PadelCore",
			// UI (HUD, menus, stats) and loading Data/*.json.
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
		});
	}
}

using UnrealBuildTool;

// The deterministic simulation: ball physics, court geometry, referee, score.
// Plain C++ with no UObjects, so it also builds standalone (Tests/Native).
// Keep dependencies to "Core" only (for the module boilerplate and automation tests).
public class PadelCore : ModuleRules
{
	public PadelCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}

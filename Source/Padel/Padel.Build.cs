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

		// MetaHuman players: the runtime IK retarget node and the C++ pose's arm IK (docs/decisions/0006-metahuman-players.md).
		PrivateDependencyModuleNames.AddRange(new string[] { "IKRig", "AnimationCore" });

		// M1 motion matching (docs/animation/MOVEMENT_FLUENCY.md): the C++ pose searches Pose Search
		// databases; FTransformTrajectory is in Engine. PadelMotionMatchingBuilder makes the assets
		// in the editor (the asset registry there).
		PublicDependencyModuleNames.Add("PoseSearch");
		PrivateDependencyModuleNames.AddRange(new string[] { "Chooser", "AssetRegistry" });
	}
}

using UnrealBuildTool;

public class DarkBlood : ModuleRules
{
	public DarkBlood(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NetCore",
			"CoreOnline",
			"Slate",
			"SlateCore",
			"DeveloperSettings",
			"AnimGraphRuntime",
			"RHI",
			"RenderCore",
			"DarkBloodRules",
		});

		// Open world: landscape (runtime queries, and the editor-only realm builder commandlet).
		PrivateDependencyModuleNames.Add("Landscape");
	}
}

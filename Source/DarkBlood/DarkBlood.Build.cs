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
		PrivateDependencyModuleNames.Add("AssetRegistry");
		// Strand hair (grooms) on authored characters, e.g. Akaza's curls and beard (docs/AKAZA_OUTFIT.md).
		PrivateDependencyModuleNames.Add("HairStrandsCore");
	}
}

using UnrealBuildTool;

public class DarkBloodTarget : TargetRules
{
	public DarkBloodTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "DarkBloodRules", "DarkBlood" });
	}
}

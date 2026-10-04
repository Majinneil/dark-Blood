using UnrealBuildTool;

public class DarkBloodEditorTarget : TargetRules
{
	public DarkBloodEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "DarkBloodRules", "DarkBlood" });
	}
}

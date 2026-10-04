// Dedicated server target. Requires an engine built from source (Launcher builds cannot build servers).
using UnrealBuildTool;

public class DarkBloodServerTarget : TargetRules
{
	public DarkBloodServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "DarkBloodRules", "DarkBlood" });
	}
}

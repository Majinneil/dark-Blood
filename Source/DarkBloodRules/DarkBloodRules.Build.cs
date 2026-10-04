// DARK BLOOD - engine-independent rules core.
// Everything except DarkBloodRulesModule.cpp is plain C++20 and is also built
// and unit-tested standalone via Tests/RulesTests/CMakeLists.txt.
using UnrealBuildTool;

public class DarkBloodRules : ModuleRules
{
	public DarkBloodRules(ReadOnlyTargetRules Target) : base(Target)
	{
		// Plain C++ sources include exactly what they use; no shared PCH.
		PCHUsage = PCHUsageMode.NoPCHs;
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}

using UnrealBuildTool;

public class AI_SetUpTool_56 : ModuleRules
{
    public AI_SetUpTool_56(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
    }
}

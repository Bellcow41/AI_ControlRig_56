using UnrealBuildTool;
using System.Collections.Generic;

public class AI_SetUpTool_56Target : TargetRules
{
    public AI_SetUpTool_56Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("AI_SetUpTool_56");
    }
}

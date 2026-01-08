using UnrealBuildTool;
using System.Collections.Generic;

public class AI_SetUpTool_56EditorTarget : TargetRules
{
    public AI_SetUpTool_56EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("AI_SetUpTool_56");
    }
}

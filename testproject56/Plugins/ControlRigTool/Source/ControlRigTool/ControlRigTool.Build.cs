using UnrealBuildTool;

public class ControlRigTool : ModuleRules
{
	public ControlRigTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"Slate", "SlateCore", "HTTP", "Json", "JsonUtilities",
			"ControlRig", "RigVM", "RigVMDeveloper",
			"ApplicationCore"
		});
			
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects", "UnrealEd", "EditorFramework", "ToolMenus",
			"AssetRegistry", "AssetTools", "ControlRigDeveloper",
			"ControlRigEditor", "EditorScriptingUtilities",
			"MeshUtilitiesCommon", "MeshUtilitiesEngine",  // 본 버텍스 정보용
			"IKRig", "IKRigEditor",  // IK Rig 생성용
			"DesktopPlatform",  // 폴더 선택 다이얼로그용
			"AnimGraph", "AnimGraphRuntime", "BlueprintGraph",  // AnimBlueprint 생성용
			"Kismet", "KismetCompiler",  // Blueprint 편집용
			"AppFramework"  // 컬러 피커용
		});
		
		// Kawaii Physics는 외부 플러그인이므로 동적 로딩 사용
		// 런타임에 플러그인이 있는지 확인 후 사용
	}
}

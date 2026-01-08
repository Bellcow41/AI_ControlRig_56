#include "ControlRigToolModule.h"
#include "ControlRigToolCommands.h"
#include "SControlRigToolWidget.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

static const FName ControlRigToolTabName("ControlRigTool");

#define LOCTEXT_NAMESPACE "FControlRigToolModule"

void FControlRigToolModule::StartupModule()
{
	FControlRigToolCommands::Register();
	
	PluginCommands = MakeShared<FUICommandList>();
	PluginCommands->MapAction(
		FControlRigToolCommands::Get().OpenToolWindow,
		FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(ControlRigToolTabName);
		}));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ControlRigToolTabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)
			[
				SNew(SControlRigToolWidget)
			];
		}))
		.SetDisplayName(LOCTEXT("TabTitle", "Control Rig Tool"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
	
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FControlRigToolModule::RegisterMenus));
	
	// API 서버 자동 시작
	StartAPIServer();
}

void FControlRigToolModule::StartAPIServer()
{
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] ========== Starting API Server =========="));
	
	// 포트 8000 사용 중인 기존 프로세스 정리
	FString KillCmd = TEXT("powershell -Command \"Get-NetTCPConnection -LocalPort 8000 -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.OwningProcess -Force -ErrorAction SilentlyContinue }\"");
	FPlatformProcess::CreateProc(TEXT("cmd.exe"), *FString::Printf(TEXT("/c %s"), *KillCmd), true, true, true, nullptr, 0, nullptr, nullptr);
	FPlatformProcess::Sleep(0.5f);  // 프로세스 종료 대기
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Cleaned up port 8000"));
	
	FString PluginDir;
	FString ServerScript;
	FString EmbeddedPython;
	
	// 1. IPluginManager를 통해 플러그인 실제 경로 가져오기
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AI_SetUpTool_56_V1"));
	if (Plugin.IsValid())
	{
		PluginDir = Plugin->GetBaseDir();
		ServerScript = PluginDir / TEXT("BoneMapping_AI/04_inference/api_server.py");
		EmbeddedPython = PluginDir / TEXT("BoneMapping_AI/python/python.exe");
		UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Plugin BaseDir: %s"), *PluginDir);
	}
	
	// 2. 못 찾으면 프로젝트 플러그인 폴더 시도
	if (!FPaths::FileExists(ServerScript))
	{
		PluginDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TEXT("AI_SetUpTool_56_V1"));
		ServerScript = PluginDir / TEXT("BoneMapping_AI/04_inference/api_server.py");
		EmbeddedPython = PluginDir / TEXT("BoneMapping_AI/python/python.exe");
		UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Try ProjectPlugins: %s"), *ServerScript);
	}
	
	// 3. 엔진 플러그인 폴더 시도
	if (!FPaths::FileExists(ServerScript))
	{
		PluginDir = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() / TEXT("AI_SetUpTool_56_V1"));
		ServerScript = PluginDir / TEXT("BoneMapping_AI/04_inference/api_server.py");
		EmbeddedPython = PluginDir / TEXT("BoneMapping_AI/python/python.exe");
		UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Try EnginePlugins: %s"), *ServerScript);
	}
	
	// 4. 프로젝트 루트 Plugins 폴더 시도
	if (!FPaths::FileExists(ServerScript))
	{
		PluginDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Plugins/AI_SetUpTool_56_V1");
		ServerScript = PluginDir / TEXT("BoneMapping_AI/04_inference/api_server.py");
		EmbeddedPython = PluginDir / TEXT("BoneMapping_AI/python/python.exe");
		UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Try ProjectDir/Plugins: %s"), *ServerScript);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] ServerScript Exists: %s"), FPaths::FileExists(ServerScript) ? TEXT("YES") : TEXT("NO"));
	
	if (!FPaths::FileExists(ServerScript))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControlRigTool] API server script not found!"));
		return;
	}
	
	// Python 경로 확인
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] EmbeddedPython: %s"), *EmbeddedPython);
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] EmbeddedPython Exists: %s"), FPaths::FileExists(EmbeddedPython) ? TEXT("YES") : TEXT("NO"));
	
	FString PythonPath = EmbeddedPython;
	
	if (!FPaths::FileExists(PythonPath))
	{
		PythonPath = TEXT("python");
		UE_LOG(LogTemp, Warning, TEXT("[ControlRigTool] Embedded Python not found, using system python"));
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Final Python: %s"), *PythonPath);
	UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] Final Script: %s"), *ServerScript);
	
	// 서버 시작 (백그라운드)
	FString Args = FString::Printf(TEXT("\"%s\""), *ServerScript);
	
	uint32 ProcessID = 0;
	FProcHandle Handle = FPlatformProcess::CreateProc(
		*PythonPath,
		*Args,
		true,   // bLaunchDetached
		true,   // bLaunchHidden
		true,   // bLaunchReallyHidden
		&ProcessID,
		0,      // Priority
		nullptr, // WorkingDirectory
		nullptr  // PipeWriteChild
	);
	
	if (Handle.IsValid())
	{
		ServerProcessHandle = Handle;
		UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] API server started (PID: %d)"), ProcessID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControlRigTool] Failed to start API server"));
	}
}

void FControlRigToolModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FControlRigToolCommands::Unregister();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ControlRigToolTabName);
	
	// API 서버 종료
	if (ServerProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(ServerProcessHandle, true);
		FPlatformProcess::CloseProc(ServerProcessHandle);
		UE_LOG(LogTemp, Log, TEXT("[ControlRigTool] API server stopped"));
	}
}

void FControlRigToolModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection("Animation");
	Section.AddMenuEntryWithCommandList(FControlRigToolCommands::Get().OpenToolWindow, PluginCommands);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FControlRigToolModule, AI_SetUpTool_56_V1)

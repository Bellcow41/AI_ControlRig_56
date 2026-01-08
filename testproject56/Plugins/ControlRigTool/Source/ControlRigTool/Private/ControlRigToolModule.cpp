#include "ControlRigToolModule.h"
#include "ControlRigToolCommands.h"
#include "SControlRigToolWidget.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

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
	// 플러그인 폴더에서 API 서버 스크립트 찾기
	// 1. 프로젝트 플러그인 폴더 시도
	FString PluginDir = FPaths::ProjectPluginsDir() / TEXT("ControlRigTool");
	FString ServerScript = PluginDir / TEXT("BoneMapping_AI/04_inference/api_server.py");
	
	// 2. 프로젝트 플러그인에 없으면 엔진 플러그인 폴더 시도
	if (!FPaths::FileExists(ServerScript))
	{
		PluginDir = FPaths::EnginePluginsDir() / TEXT("ControlRigTool");
		ServerScript = PluginDir / TEXT("BoneMapping_AI/04_inference/api_server.py");
	}
	
	// 3. 그래도 없으면 프로젝트 루트 시도 (개발용)
	if (!FPaths::FileExists(ServerScript))
	{
		ServerScript = FPaths::ProjectDir() / TEXT("Plugins/ControlRigTool/BoneMapping_AI/04_inference/api_server.py");
	}
	
	if (!FPaths::FileExists(ServerScript))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControlRigTool] API server script not found: %s"), *ServerScript);
		return;
	}
	
	// Python 경로 (시스템 Python 사용)
	FString PythonPath = TEXT("C:/Users/Administrator/AppData/Local/Programs/Python/Python311/python.exe");
	
	// 대안 경로들 시도
	if (!FPaths::FileExists(PythonPath))
	{
		PythonPath = TEXT("python");  // PATH에서 찾기
	}
	
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

IMPLEMENT_MODULE(FControlRigToolModule, ControlRigTool)

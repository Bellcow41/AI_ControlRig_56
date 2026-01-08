#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"

class FControlRigToolModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
private:
	void RegisterMenus();
	void StartAPIServer();
	
	TSharedPtr<class FUICommandList> PluginCommands;
	FProcHandle ServerProcessHandle;
};

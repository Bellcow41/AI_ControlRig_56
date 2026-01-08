#pragma once
#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FControlRigToolCommands : public TCommands<FControlRigToolCommands>
{
public:
	FControlRigToolCommands()
		: TCommands<FControlRigToolCommands>(
			TEXT("ControlRigTool"),
			NSLOCTEXT("Contexts", "ControlRigTool", "Control Rig Tool"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()) {}

	virtual void RegisterCommands() override;
	TSharedPtr<FUICommandInfo> OpenToolWindow;
};

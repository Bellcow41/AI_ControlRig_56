#include "ControlRigToolCommands.h"

#define LOCTEXT_NAMESPACE "FControlRigToolCommands"

void FControlRigToolCommands::RegisterCommands()
{
	UI_COMMAND(OpenToolWindow, "Control Rig Tool", "Open Control Rig Tool", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

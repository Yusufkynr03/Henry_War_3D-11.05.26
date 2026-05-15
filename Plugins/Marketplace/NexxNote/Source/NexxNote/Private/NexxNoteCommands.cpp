// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "NexxNoteCommands.h"
#include "NexxNoteStyle.h"
#include "Logging/LogMacros.h"

#define LOCTEXT_NAMESPACE "FNexxNoteCommands"

void FNexxNoteCommands::RegisterCommands()
{
	// Ana plugin komutunu kaydet
	UI_COMMAND(PluginAction, "NexxNoteMain", "Execute NexxNote main action", EUserInterfaceActionType::Button, FInputChord());
	
	// Sketch modu komutu
	UI_COMMAND(
		ToggleSketchMode,
		"Sketch",
		"Allows you to draw on the Event Graph",
		EUserInterfaceActionType::ToggleButton,
		FInputChord()
	);
	
	// Not modu komutu
	UI_COMMAND(
		ToggleNoteMode,
		"Add Note",
		"Allows you to add notes to the Event Graph",
		EUserInterfaceActionType::ToggleButton,
		FInputChord()
	);
	
	// Renk seçici komutu
	UI_COMMAND(
		ColorPickerAction,
		"Color Picker",
		"Drawing Color and Thickness",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
}

#undef LOCTEXT_NAMESPACE 
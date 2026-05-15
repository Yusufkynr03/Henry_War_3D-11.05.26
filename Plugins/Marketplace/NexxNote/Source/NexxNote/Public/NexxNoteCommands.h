// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "NexxNoteStyle.h"

/**
 * NexxNote için komutlar
 */
class FNexxNoteCommands : public TCommands<FNexxNoteCommands>
{
public:
	FNexxNoteCommands()
		: TCommands<FNexxNoteCommands>(
			TEXT("NexxNote"), // Context name
			NSLOCTEXT("Contexts", "NexxNote", "NexxNote Plugin"), // Context description
			NAME_None, // Parent context
			FNexxNoteStyle::GetStyleSetName() // Icon Style
		)
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

	// Komutların register edilip edilmediğini kontrol eder
	static bool IsRegistered()
	{
		return TCommands<FNexxNoteCommands>::IsRegistered();
	}

	// Komutlar
	TSharedPtr<FUICommandInfo> PluginAction;
	TSharedPtr<FUICommandInfo> ToggleSketchMode;
	TSharedPtr<FUICommandInfo> ToggleNoteMode;
	TSharedPtr<FUICommandInfo> ColorPickerAction;
}; 
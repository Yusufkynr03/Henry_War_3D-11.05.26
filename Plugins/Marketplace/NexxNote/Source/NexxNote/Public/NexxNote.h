// Copyright 2025 Onur Altuntaş. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "Framework/Commands/UICommandList.h"
#include "LevelEditor.h"
#include "Styling/AppStyle.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "NexxNoteCommands.h"
#include "NexxNoteStyle.h"
#include "NexxNoteGraphNodeFactory.h"
#include "NexxNoteGraphPanelExtension.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "SNexxNoteColorPicker.h"

class FToolBarBuilder;
class FMenuBuilder;

class FNexxNoteModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** Returns singleton instance, loading the module on demand if needed */
	static inline FNexxNoteModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FNexxNoteModule>("NexxNote");
	}
	
	/** Button actions */
	void OnSketchButtonClicked();
	void OnNoteModeButtonClicked();
	void PluginButtonClicked();
	
	/** Canvas callbacks */
	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);
	
	/** Checks for mode states */
	bool IsSketchModeActive() const;
	bool IsNoteModeActive() const;

	/** This function will be bound to Command (by default it will bring up plugin window) */
	FLinearColor GetDrawingColor() const
	{
		return GraphPanelExtension.IsValid() ? 
			GraphPanelExtension->GetCurrentDrawingColor() : FLinearColor::Yellow;
	}

	/** Engine tam olarak başlatıldığında çağrılacak fonksiyon */
	void OnPostEngineInit();

private:
	/** Register commands for menus */
	void RegisterMenus();
	
	/** Register blueprint toolbar extension */
	void RegisterBlueprintToolbarExtension();
	
	/** Unregister blueprint toolbar extension */
	void UnregisterBlueprintToolbarExtension();
	
	/** Add toolbar buttons to the toolbar builder */
	void AddToolbarButtons(FToolBarBuilder& ToolbarBuilder);
	
	/** Update button visual states */
	void UpdateButtonStates();
	
	/** UI Command List - for shortcuts and actions */
	TSharedPtr<FUICommandList> UICommandList;
	
	/** Plugin commands list */
	TSharedPtr<FUICommandList> PluginCommands;
	
	/** Blueprint editor toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;
	
	/** Extension for the graph panel */
	TSharedPtr<FNexxNoteGraphPanelExtension> GraphPanelExtension;
	
	/** Graph node factory */
	TSharedPtr<FNexxNoteGraphNodeFactory> GraphNodeFactory;
	
	/** Tab name */
	static FName NexxNoteTabName;
	
	/** Flags for mode states */
	bool bSketchModeActive = false;
	bool bNoteModeActive = false;
	
	TSharedPtr<SNexxNoteColorPicker> PersistentColorPickerWidget;
};

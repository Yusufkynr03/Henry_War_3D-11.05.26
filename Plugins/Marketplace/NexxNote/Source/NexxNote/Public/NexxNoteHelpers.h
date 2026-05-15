// Copyright 2025 Onur Altuntaş. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

namespace NexxNoteHelpers
{
    /**
     * Recursively finds all widgets starting from a root widget
     *
     * @param Root       The root widget to start the search from
     * @param OutWidgets Array that will be filled with all found widgets
     */
    inline void FindAllWidgetsRecursive(TSharedRef<SWidget, ESPMode::ThreadSafe> Root, 
                                  TArray<TSharedRef<SWidget, ESPMode::ThreadSafe>>& OutWidgets)
    {
        OutWidgets.Add(Root);
        
        FChildren* Children = Root->GetChildren();
        if (!Children)
        {
            return;
        }
        
        for (int32 i = 0; i < Children->Num(); ++i)
        {
            TSharedRef<SWidget, ESPMode::ThreadSafe> Child = Children->GetChildAt(i);
            FindAllWidgetsRecursive(Child, OutWidgets);
        }
    }
} 
// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "SGraphNode.h"
#include "EdGraph/EdGraphNode.h"
#include "NodeFactory.h"

/**
 * Factory for creating custom graph node widgets
 */
class FNexxNoteGraphNodeFactory : public FGraphPanelNodeFactory
{
public:
	// Begin FGraphPanelNodeFactory interface
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override;
	// End FGraphPanelNodeFactory interface
}; 
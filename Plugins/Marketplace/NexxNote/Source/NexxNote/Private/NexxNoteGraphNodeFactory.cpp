// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "NexxNoteGraphNodeFactory.h"
#include "UNexxSketchNode.h"
#include "UNexxNoteNode.h"
#include "SGraphNodeSketch.h"
#include "SGraphNodeNote.h"
#include "EdGraph/EdGraphNode.h"
#include "SGraphNode.h"
#include "Templates/SharedPointer.h"
#include "NodeFactory.h"
#include "K2Node.h"

// Cache sistemini devre dışı bırakıyoruz
// static TMap<FGuid, TWeakPtr<SGraphNode>> SketchNodeWidgetCache;

TSharedPtr<SGraphNode> FNexxNoteGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	// Sketch node kontrolü
	if (UNexxSketchNode* NexxSketchNode = Cast<UNexxSketchNode>(InNode))
	{
		// Sketch node için widget oluştur
		return SNew(SGraphNodeSketch, NexxSketchNode);
	}
	
	// Not node kontrolü
	if (UNexxNoteNode* NexxNoteNode = Cast<UNexxNoteNode>(InNode))
	{
		// Not node için widget oluştur
		return SNew(SGraphNodeNote, NexxNoteNode);
	}
	
	// Not a node we handle
	return nullptr;
} 
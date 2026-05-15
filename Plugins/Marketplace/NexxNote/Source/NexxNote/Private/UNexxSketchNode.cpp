// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "UNexxSketchNode.h"
#include "Kismet2/BlueprintEditorUtils.h"

UNexxSketchNode::UNexxSketchNode()
{
	// Varsayılan değerler
	
	// NodeGuid'i başlat
	NodeGuid = FGuid::NewGuid();
}

void UNexxSketchNode::MoveSketchPoint(const FVector2D& NewPosition)
{
	// Node'u değişiklik yapılacağını işaretle
	Modify();
	
	// Node pozisyonunu güncelle
	NodePosX = NewPosition.X;
	NodePosY = NewPosition.Y;

	// Blueprint editörünü güncelle
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void UNexxSketchNode::MoveAllPoints(const FVector2D& Delta)
{
	// Node'u değişiklik yapılacağını işaretle
	Modify();
	
	// Tüm çizim noktalarını taşı
	for (FVector2D& Point : DrawPoints)
	{
		Point += Delta;
	}
	
	// Node pozisyonunu da güncelle
	NodePosX += Delta.X;
	NodePosY += Delta.Y;
	
	// Blueprint editörünü güncelle
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
} 
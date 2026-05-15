// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "SketchNode.h"
#include "UNexxSketchNode.generated.h"

/**
 * SketchNode'dan türetilmiş ve ek özellikler içeren bir sınıf.
 * SGraphNodeSketch tarafından kullanılır.
 */
UCLASS()
class NEXXNOTE_API UNexxSketchNode : public USketchNode
{
	GENERATED_BODY()

public:
	UNexxSketchNode();

	// Çizim noktalarını taşı
	void MoveSketchPoint(const FVector2D& NewPosition);

	// Node noktasını ve çizim noktalarını eşzamanlı taşı
	void MoveAllPoints(const FVector2D& Delta);
}; 
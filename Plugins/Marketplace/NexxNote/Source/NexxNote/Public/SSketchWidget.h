// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "GraphEditor.h"
#include "GraphEditorDragDropAction.h"
#include "SGraphPanel.h"
#include "UNexxSketchNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Misc/Attribute.h"

class UEdGraph;

/**
 * Widget for drawing and interacting with sketches in the Blueprint Event Graph
 */
class NEXXNOTE_API SSketchWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSketchWidget)
		: _GraphPanel(nullptr)
	{}
		SLATE_ARGUMENT(SGraphPanel*, GraphPanel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End SWidget interface

	// Enable/disable sketch mode
	void SetSketchModeActive(bool bActive);
	bool IsSketchModeActive() const { return bSketchModeActive; }

	// Enable/disable note mode
	void SetNoteModeActive(bool bActive);
	bool IsNoteModeActive() const { return bNoteModeActive; }

	// Görünürlük ayarları için yardımcı fonksiyon
	EVisibility GetSketchVisibility() const;

private:
	// The parent graph panel that owns this widget
	SGraphPanel* GraphPanel;
	
	// Overlay widget for drawing
	TSharedPtr<SOverlay> OverlayWidget;

	// Is sketch mode active
	bool bSketchModeActive;

	// Is note mode active
	bool bNoteModeActive;

	// Is currently drawing a sketch
	bool bIsDrawing;

	// Current sketch points being drawn
	TArray<FVector2D> CurrentSketchPoints;

	// Current sketch color and thickness
	FLinearColor CurrentSketchColor;
	float CurrentSketchThickness;

	// Finish the current sketch and create a node
	void FinishSketch();

	// Add a point to the current sketch
	void AddSketchPoint(const FVector2D& Point);

	// Clear the current sketch
	void ClearCurrentSketch();

	// Helper to convert screen space position to graph space
	FVector2D ScreenSpaceToGraphSpace(const FVector2D& ScreenSpacePosition) const;
}; 
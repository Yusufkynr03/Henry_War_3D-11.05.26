// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Misc/Attribute.h"
#include "Widgets/Text/STextBlock.h"

class SGraphPanel;
class UEdGraph;
class UNexxSketchNode;

// Delegat tanımları
DECLARE_DELEGATE_OneParam(FOnSketchCompleted, const TArray<FVector2D>&);
DECLARE_DELEGATE(FOnSketchModeDeactivated);
DECLARE_DELEGATE_OneParam(FOnDrawingColorChanged, FLinearColor);
DECLARE_DELEGATE_OneParam(FOnDrawingThicknessChanged, float);

// Bu sınıf, doğrudan Blueprint Event Graph'inde çizim yapmak için kullanılacak
class NEXXNOTE_API SNexxNoteDrawingOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNexxNoteDrawingOverlay) {}
	SLATE_END_ARGS()

	/** Slate widget construction */
	void Construct(const FArguments& InArgs, SGraphPanel* InGraphPanel);

	/** Default constructor */
	SNexxNoteDrawingOverlay();

	/** Sets the sketch mode active state */
	void SetSketchModeActive(bool bActive);

	/** Gets the sketch mode active state */
	bool IsSketchModeActive() const { return bSketchModeActive; }

	/** Sets the note mode active state */
	void SetNoteModeActive(bool bActive);

	/** Gets the note mode active state */
	bool IsNoteModeActive() const { return bNoteModeActive; }

	/** Sets the graph panel reference */
	void SetGraphPanel(SGraphPanel* InGraphPanel) { GraphPanel = InGraphPanel; }

	/** Called when sketch mode is deactivated */
	FOnSketchModeDeactivated OnSketchModeDeactivated;

	/** Sets the draw color */
	void SetDrawColor(const FLinearColor& InColor) { DrawColor = InColor; }

	/** Sets the draw thickness */
	void SetDrawThickness(float InThickness) { DrawThickness = InThickness; }

	// Mouse event handlers
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Override HitTest to control when overlay widgets receive mouse events */
	virtual bool HitTest(const FGeometry& MyGeometry, FVector2D InAbsoluteCursorPosition);
	
	/** Tick to update mouse position and control drawing state */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	
	// Çizim özelliklerini ayarlama fonksiyonları
	void SetDrawingColor(const FLinearColor& NewColor);
	void SetDrawingThickness(float NewThickness);
	FLinearColor GetDrawingColor() const { return DrawColor; }
	float GetDrawingThickness() const { return DrawThickness; }
	
	// Panel alanı içinde olup olmadığımızı hesapla
	bool IsPositionInsidePanel(const FVector2D& ScreenPosition) const;

	// Panel alanı içinde olup olmadığını geometri ile kontrol et (daha hassas)
	bool IsPositionInsidePanel(const FGeometry& MyGeometry, const FVector2D& ScreenPosition) const;

	// Delegateler
	FOnSketchCompleted OnSketchCompleted;
	FOnDrawingColorChanged OnDrawingColorChanged;
	FOnDrawingThicknessChanged OnDrawingThicknessChanged;

private:
	/** Starts drawing at the given mouse position */
	void StartDrawing(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Finishes the current drawing and creates a sketch node */
	void FinishDrawing();

	/** Optimize the draw points by removing points that are too close to each other */
	void OptimizeDrawPoints(const TArray<FVector2D>& InPoints, TArray<FVector2D>& OutPoints, float MinDistance = 5.0f) const;

	/** Smooth the draw points using Catmull-Rom spline interpolation */
	void SmoothDrawPoints(const TArray<FVector2D>& InPoints, TArray<FVector2D>& OutPoints, int32 Subdivisions = 3) const;

	/** Catmull-Rom spline hesaplama */
	FVector2D CatmullRomSpline(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, float T) const;

	/** Graph koordinatını panel koordinatına dönüştürme (KALDIRILIYOR) */
	FVector2D GraphCoordToPanelCoord(const FVector2D& GraphCoord) const;

	/** Panel koordinatını graph koordinatına dönüştürme (KALDIRILIYOR) */
	FVector2D PanelCoordToGraphCoord(const FVector2D& PanelCoord) const;

	/** Paint interface - draws the current sketch */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	/** Shift tuşunun basılı olup olmadığını kontrol et */
	bool IsShiftKeyDown() const;

protected:
	/** Reference to the graph panel this overlay is attached to */
	SGraphPanel* GraphPanel;
	
	/** Whether sketch mode is active */
	bool bSketchModeActive;

	/** Whether note mode is active */
	bool bNoteModeActive;
	
	/** Whether we are currently drawing */
	bool bDrawing;
	
	/** Whether the mouse is currently inside the panel bounds */
	bool bIsMouseInsidePanel;
	
	/** Whether the left mouse button is currently pressed */
	bool bIsMouseLeftButtonDown;

	/** Whether the Shift key was pressed during the last mouse button up */
	bool bWasShiftKeyDown;

	/** The current draw color */
	FLinearColor DrawColor;

	/** The current draw thickness */
	float DrawThickness;
	
	/** The current draw points */
	TArray<FVector2D> DrawPoints;
	
	/** Shift ile yapılan farklı çizim parçalarının başlangıç indeksleri */
	TArray<int32> StrokeBreakIndices;
	
	/** Whether the widget should tick (update) every frame */
	bool bCanTick;
	
    /** Son kayıt edilen fare pozisyonu */
    FVector2D LastMousePos;

    /** Durum bilgisi texti için TextBlock widget'ı */
    TSharedPtr<STextBlock> StatusTextBlock;
    
    /** Shift tuşu durumu için TextBlock widget'ı */
    TSharedPtr<STextBlock> ShiftStateTextBlock;

	// Panel koordinatlarını Graph koordinatlarına dönüştüren yardımcı fonksiyon
	FVector2D PanelToGraphCoord(const FVector2D& PanelCoord, float ZoomFactor, const FVector2D& ViewOff) const;
}; 
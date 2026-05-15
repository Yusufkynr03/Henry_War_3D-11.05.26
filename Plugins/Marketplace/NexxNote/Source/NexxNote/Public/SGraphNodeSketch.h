// Copyright 2025 Onur Altuntaş. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Types/SlateStructs.h"
#include "UNexxSketchNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

/**
 * Sketch node için Slate widget
 * Blueprint editöründe çizim yapabilmek için kullanılır
 */
class NEXXNOTE_API SGraphNodeSketch : public SGraphNode {
public:
  SLATE_BEGIN_ARGS(SGraphNodeSketch) {}
  SLATE_END_ARGS()

  // Yapıcı - Sketch node'un başlatılması
  void Construct(const FArguments &InArgs, UNexxSketchNode *InNode);

  // SGraphNode override'ları
  virtual void UpdateGraphNode();
  virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox);
  virtual TSharedPtr<SGraphNode>
  GetNodeUnderMouse(const FGeometry &MyGeometry,
                    const FPointerEvent &MouseEvent);
  virtual int32 OnPaint(const FPaintArgs &Args,
                        const FGeometry &AllottedGeometry,
                        const FSlateRect &MyCullingRect,
                        FSlateWindowElementList &OutDrawElements, int32 LayerId,
                        const FWidgetStyle &InWidgetStyle,
                        bool bParentEnabled) const override;
  virtual FVector2D ComputeDesiredSize(float) const;
  virtual void SetGraphNode(UEdGraphNode *InNode);
  virtual void EndUserInteraction() const;
  virtual void MoveTo(const FVector2f &NewPosition, FNodeSet &NodeFilter,
                      bool bMarkDirty = true);
  virtual void Tick(const FGeometry &AllottedGeometry,
                    const double InCurrentTime, const float InDeltaTime);
  virtual void OnMouseEnter(const FGeometry &MyGeometry,
                            const FPointerEvent &MouseEvent);
  virtual void OnMouseLeave(const FPointerEvent &MouseEvent);
  virtual FReply OnMouseButtonDown(const FGeometry &MyGeometry,
                                   const FPointerEvent &MouseEvent);
  virtual FReply OnMouseButtonUp(const FGeometry &MyGeometry,
                                 const FPointerEvent &MouseEvent);
  virtual FReply OnMouseMove(const FGeometry &MyGeometry,
                             const FPointerEvent &MouseEvent);
  virtual FReply OnDragDetected(const FGeometry &MyGeometry,
                                const FPointerEvent &MouseEvent);
  virtual bool HitTest(const FGeometry &MyGeometry,
                       FVector2D InAbsoluteCursorPosition) const override;
  virtual bool IsUserInteractionEnabled() const override;
  virtual FVector2D GetDesiredSizeForMarquee() const override;
  virtual FVector2D GetPosition() const override;
  virtual const FSlateBrush *GetShadowBrush(bool bSelected) const override;

  // Yardımcı fonksiyonlar
  void DrawSketch(const FGeometry &AllottedGeometry,
                  FSlateWindowElementList &OutDrawElements, int32 LayerId,
                  const FWidgetStyle &InWidgetStyle, bool bParentEnabled,
                  const FLinearColor &Color) const;
  void GetBoundingBoxForSketch(FVector2D &OutMinPoint, FVector2D &OutMaxPoint,
                               float Padding) const;
  bool IsPointInsideSketch(const FVector2D &Point) const;
  void ForceRefreshNode() const;
  TSharedPtr<SGraphPanel> FindParentGraphPanel() const;
  FVector2D GetSketchSize() const;
  FBox2D GetBoundingBoxForSketch() const;

protected:
  // Sketch node referansı
  UNexxSketchNode *SketchNodePtr;

  // Hover durumunu göster
  bool bIsHoveringOverSketch;

  // Node büyüklüğü
  mutable FVector2D CurrentNodeSize;
  float TargetSizeMultiplier;
  float CurrentSizeMultiplier;

  // Minimum ve maksimum node büyüklükleri
  static const FVector2D MinSize;
  static const FVector2D MaxSize;

  // Çizim özellikleri
  FLinearColor DrawColor;
  float LineThickness;
  TArray<FVector2D> DrawPoints;
};
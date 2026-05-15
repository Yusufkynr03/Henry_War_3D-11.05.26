// Copyright 2025 Onur Altuntaş. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "UNexxSketchNode.h"

/**
 * NexxNote plugin tarafından özel olarak oluşturulmuş bir çizim (Sketch) nodu
 * widget'ı Kullanıcı tarafından çizilmiş çizimler için görsel ve etkileşim
 * sağlar
 */
class NEXXNOTE_API SGraphNodeSketch : public SGraphNode {
public:
  SLATE_BEGIN_ARGS(SGraphNodeSketch) {}
  SLATE_END_ARGS()

  // Constructor metodu
  void Construct(const FArguments &InArgs, UNexxSketchNode *InNode);

  // SWidget arayüzü
  virtual void UpdateGraphNode();
  virtual void CreateBorderBackgroundForNode();
  virtual void CreateRenderingResources();

  // Fare ve etkileşim metodları
  virtual FReply OnMouseMove(const FGeometry &MyGeometry,
                             const FPointerEvent &MouseEvent);
  virtual FReply OnMouseButtonDown(const FGeometry &MyGeometry,
                                   const FPointerEvent &MouseEvent);
  virtual FReply OnMouseButtonUp(const FGeometry &MyGeometry,
                                 const FPointerEvent &MouseEvent);
  virtual void OnMouseEnter(const FGeometry &MyGeometry,
                            const FPointerEvent &MouseEvent);
  virtual void OnMouseLeave(const FPointerEvent &MouseEvent);
  virtual FVector2D ComputeDesiredSize(float) const;
  virtual const FSlateBrush *GetShadowBrush(bool bSelected) const override;
  virtual bool HitTest(const FGeometry &MyGeometry,
                       FVector2D InAbsoluteCursorPosition) const;
  virtual void Tick(const FGeometry &AllottedGeometry,
                    const double InCurrentTime, const float InDeltaTime);

  // Çizim ve render metodları
  virtual int32 OnPaint(const FPaintArgs &Args,
                        const FGeometry &AllottedGeometry,
                        const FSlateRect &MyCullingRect,
                        FSlateWindowElementList &OutDrawElements, int32 LayerId,
                        const FWidgetStyle &InWidgetStyle,
                        bool bParentEnabled) const;
  virtual void DrawSketch(const FGeometry &AllottedGeometry,
                          FSlateWindowElementList &OutDrawElements,
                          int32 LayerId, const FWidgetStyle &InWidgetStyle,
                          bool bParentEnabled, const FLinearColor &Color) const;

  // Node manipülasyon metodları
  virtual void MoveTo(const FVector2f &NewPosition, FNodeSet &NodeFilter,
                      bool bMarkDirty);
  virtual void EndUserInteraction() const;
  virtual void SetGraphNode(UEdGraphNode *InNode);
  virtual FVector2D GetPosition() const;

  // Yardımcı metodlar
  TSharedPtr<SGraphPanel> FindParentGraphPanel() const;
  FVector2D GetSketchSize() const;
  void GetBoundingBoxForSketch(FVector2D &OutMinPoint, FVector2D &OutMaxPoint,
                               float Padding = 0.0f) const;
  FBox2D GetBoundingBoxForSketch() const;
  bool IsPointInsideSketch(const FVector2D &Point) const;
  void ForceRefreshNode();

protected:
  // SketchNode referansı (özel node tipi)
  UNexxSketchNode *SketchNodePtr;

  // Hover durumu - fare çizim üzerinde mi?
  mutable bool bIsHoveringOverSketch;

  // Animasyon için boyut değişkenleri
  float CurrentSizeMultiplier;
  float TargetSizeMultiplier;
  FVector2D CurrentNodeSize;

  // Statik değişkenler ve sabitler
  static const FVector2D MinSize;
  static const FVector2D MaxSize;
  static constexpr float SizeLerpSpeed = 5.0f;
};
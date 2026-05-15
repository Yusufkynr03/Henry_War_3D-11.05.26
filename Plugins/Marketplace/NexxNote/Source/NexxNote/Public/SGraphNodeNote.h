// Copyright 2025 Onur Altuntaş. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Types/SlateStructs.h"
#include "UNexxNoteNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"

// Forward declaration
class SScrollInterceptor;

/**
 * Not node için Slate widget
 * Blueprint Event Graph'inde not ekleme ve düzenleme özelliği sağlar
 */
class NEXXNOTE_API SGraphNodeNote : public SGraphNode {
public:
  SLATE_BEGIN_ARGS(SGraphNodeNote) {}
  SLATE_END_ARGS()

  // Yapıcı - Not node'un başlatılması
  void Construct(const FArguments &InArgs, UNexxNoteNode *InNode);

  // SGraphNode override'ları
  virtual void UpdateGraphNode() override;
  virtual void
  CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
  virtual int32 OnPaint(const FPaintArgs &Args,
                        const FGeometry &AllottedGeometry,
                        const FSlateRect &MyCullingRect,
                        FSlateWindowElementList &OutDrawElements, int32 LayerId,
                        const FWidgetStyle &InWidgetStyle,
                        bool bParentEnabled) const override;
  virtual FVector2D ComputeDesiredSize(float) const override;
  virtual void MoveTo(const FVector2f &NewPosition, FNodeSet &NodeFilter,
                      bool bMarkDirty = true) override;
  virtual void EndUserInteraction() const override;
  virtual bool IsNameReadOnly() const override { return false; }

protected:
  // Not içeriğini düzenlendiğinde çağrılır
  void OnNoteTextChanged(const FText &NewText);

  // Not içeriğinin kayıt edilmesi için commit edildiğinde çağrılır
  void OnNoteTextCommitted(const FText &NewText, ETextCommit::Type CommitType);

  // Not boyutu değiştiğinde çağrılır
  void OnNoteResized(const FVector2D &NewSize);

  // Metin alanına tıklandığında çağrılır
  FReply OnTextAreaClicked(const FGeometry &MyGeometry,
                           const FPointerEvent &MouseEvent);

  // Not görsellerini oluştur
  virtual void CreateNoteComponents();

  // Not büyüklüğünü değiştir
  void ResizeNote(const FVector2D &NewSize);

private:
  // Not node referansı
  UNexxNoteNode *NoteNodePtr;

  // Not kutusu ve görseli
  TSharedPtr<SVerticalBox> NoteBox;
  TSharedPtr<SImage> NoteImage;

  // Metin içeriği widget'ı
  TSharedPtr<SMultiLineEditableText> TextContent;

  // Scroll box referansı
  TSharedPtr<SScrollBox> ScrollBox;

  // Scroll interceptor widget
  TSharedPtr<SScrollInterceptor> ScrollInterceptor;

  // Parent widget hiyerarşisinden SGraphPanel'i bulur
  TSharedPtr<SGraphPanel> FindParentGraphPanel() const;

  // Not görseli
  const FSlateBrush *NoteFullBrush;

  // Not büyüklüğü
  mutable FVector2D CurrentNodeSize;

  // Hover durumu
  bool bIsHovering;

  // Placeholder durumu
  bool bIsPlaceholderActive;

  // Not görseli boyutları
  float NoteImageWidth = 350.0f;
  float NoteImageHeight = 250.0f;
};
// Copyright 2025 Onur Altuntaş. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

// Renk değişimi için delegat
DECLARE_DELEGATE_OneParam(FOnColorSelected, const FLinearColor &);
DECLARE_DELEGATE_OneParam(FOnThicknessSelected, float);

/**
 * Blueprint Editörü için özel renk ve kalınlık seçici widget
 */
class NEXXNOTE_API SNexxNoteColorPicker : public SCompoundWidget {
public:
  SLATE_BEGIN_ARGS(SNexxNoteColorPicker)
      : _InitialColor(
            FLinearColor::Yellow) // Başlangıç rengini Yellow olarak değiştirdim
        ,
        _InitialThickness(3.0f) {}
  SLATE_ATTRIBUTE(FLinearColor, InitialColor)
  SLATE_ATTRIBUTE(float, InitialThickness)
  SLATE_EVENT(FOnColorSelected, OnColorSelected)
  SLATE_EVENT(FOnThicknessSelected, OnThicknessSelected)
  SLATE_END_ARGS()

  /** Widget'ı yapılandır */
  void Construct(const FArguments &InArgs);

  // Şu anki seçili rengi döndür
  FLinearColor GetSelectedColor() const { return SelectedColor; }

  // Şu anki seçili kalınlığı döndür
  float GetSelectedThickness() const { return SelectedThickness; }

  // Tüm widget'ı güncellemek için zorlayıcı yenileme
  void ForceRefresh();

private:
  // Renk seçimi için kullanılacak basit renkler
  TArray<FLinearColor> PresetColors;

  // Şu anki seçili renk
  FLinearColor SelectedColor;

  // Şu anki seçili kalınlık
  float SelectedThickness;

  // Kalınlık değerini gösteren metin için TAttribute
  TAttribute<FText> ThicknessTextAttr;

  // Renk seçimi delegatı
  FOnColorSelected OnColorSelected;

  // Kalınlık seçimi delegatı
  FOnThicknessSelected OnThicknessSelected;

  // Yuvarlak köşeli renk brush'larını saklamak için
  TArray<TSharedPtr<FSlateBrush>> PresetBrushes;

  // Kenarlık için brush'lar
  TArray<TSharedPtr<FSlateBrush>> PresetBorderBrushes;

  // Seçim için parlayan brush'lar (glow efekti)
  TArray<TSharedPtr<FSlateBrush>> SelectionGlowBrushes;

  // Highlight efekt brush'ları
  TSharedPtr<FSlateBrush> HighlightBrush;
  TSharedPtr<FSlateBrush> SelectionHighlightBrush;
  TSharedPtr<FSlateBrush> InnerShadowBrush;

  // Önizleme kutusu için dinamik oval brush
  TSharedPtr<FSlateRoundedBoxBrush> PreviewColorBrush;

  // Icon brushes for buttons
  TSharedPtr<FSlateBrush> SketchIconBrush;
  TSharedPtr<FSlateBrush> ColorChangerIconBrush;
  TSharedPtr<FSlateBrush> NoteIconBrush;

  // Renk butonunu oluştur
  TSharedRef<SWidget> MakeColorButton(const FLinearColor &Color);

  // Renk butonuna tıklandığında
  FReply OnColorButtonClicked(const FLinearColor &Color);

  // Kalınlık değiştiğinde
  void OnThicknessChanged(float NewValue);

  // Renk preview kutusuna tıklanınca
  FReply OnColorBlockClicked(const FGeometry &MyGeometry,
                             const FPointerEvent &MouseEvent);

  FReply OnOpenFullColorPickerClicked();

  void OpenColorPicker();

  // Handler for live color changes during interaction
  void HandleColorChanged(FLinearColor NewColor);

  // Handler for when a new color is committed via the picker
  void HandleColorCommitted(FLinearColor NewColor);

  // Refreshes the preview block after an interactive pick ends
  void RefreshColorBlock();

  // Önizleme kutusu brush'ını günceller
  void UpdatePreviewColorBrush();

  bool bIsColorButtonHovered = false;

  // Function to get the menu content for SMenuAnchor
  TSharedRef<SWidget> GetMenuContent();
};
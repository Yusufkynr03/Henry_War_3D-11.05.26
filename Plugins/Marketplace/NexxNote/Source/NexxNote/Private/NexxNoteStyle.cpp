// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "NexxNoteStyle.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NexxNoteCommands.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

#define IMAGE_BRUSH(RelativePath, ...)                                         \
  FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")),     \
                   __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...)                                           \
  FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")),       \
                 __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...)                                        \
  FSlateBorderBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")),    \
                    __VA_ARGS__)

// Style singleton
TSharedPtr<FSlateStyleSet> FNexxNoteStyle::StyleInstance = NULL;

void FNexxNoteStyle::Initialize() {
  if (!StyleInstance.IsValid()) {
    StyleInstance = Create();
    FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
  }
}

void FNexxNoteStyle::Shutdown() {
  FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
  ensure(StyleInstance.IsUnique());
  StyleInstance.Reset();
}

FName FNexxNoteStyle::GetStyleSetName() {
  static FName StyleSetName(TEXT("NexxNoteStyle"));
  return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

// Renk değiştirici ikonu için özel boyut
const FVector2D ColorChangerIcon(20.0f,
                                 20.0f); // 20x20'den 16x16'ya küçültüyorum

// NOT: Boyutları buradan değiştirebilirsiniz. Daha büyük veya küçük değerler
// kullanabilirsiniz. Tek parça not görüntüsü için boyut tanımı
const FVector2D NoteFullSize(750.0f,
                             250.0f); // Genişlik, Yükseklik - tek parça görsel

// Tek parça not görüntüsü için dosya yolu
static const FString NoteFullPngPath = TEXT("Resources/NoteFull");

// Tek parça not görüntüsü için asset yolu
static const FString NoteFullAssetPath = TEXT("/NexxNote/NoteFull.NoteFull");

TSharedRef<FSlateStyleSet> FNexxNoteStyle::Create() {
  TSharedRef<FSlateStyleSet> Style =
      MakeShareable(new FSlateStyleSet("NexxNoteStyle"));
  Style->SetContentRoot(
      IPluginManager::Get().FindPlugin("NexxNote")->GetBaseDir());

  // İkon yollarını ve boyutlarını tanımla
  const FString IconDir = TEXT("Resources");

  // İkonların tam dosya yolları
  FString SketchIconPath = FPaths::Combine(Style->GetContentRootDir(), IconDir,
                                           TEXT("Sketchicon.png"));
  FString NoteIconPath = FPaths::Combine(Style->GetContentRootDir(), IconDir,
                                         TEXT("Noteicon.png"));
  FString ColorIconPath = FPaths::Combine(Style->GetContentRootDir(), IconDir,
                                          TEXT("Colorchangericon.png"));

  // İkonların varlığını kontrol et
  bool bSketchIconExists = FPaths::FileExists(SketchIconPath);
  bool bNoteIconExists = FPaths::FileExists(NoteIconPath);
  bool bColorIconExists = FPaths::FileExists(ColorIconPath);

  // ÖNEMLİ: Buton stillerini tanımla - hem SketchButton hem de SketchIcon
  // olarak (eski ve yeni adlar) FNexxNote.cpp'deki AddToolbarButtons
  // fonksiyonunda SketchButton adını kullanıyoruz
  Style->Set("NexxNote.SketchButton",
             new FSlateImageBrush(SketchIconPath, Icon40x40));
  Style->Set("NexxNote.SketchButton.Small",
             new FSlateImageBrush(SketchIconPath, Icon20x20));

  // İsim uyumsuzluğunu da çöz - iki tane de isim ekle
  Style->Set("NexxNote.SketchIcon",
             new FSlateImageBrush(SketchIconPath, Icon40x40));
  Style->Set("NexxNote.SketchIcon.Small",
             new FSlateImageBrush(SketchIconPath, Icon20x20));

  // Not butonları için de aynı işlemi yap
  Style->Set("NexxNote.NoteButton",
             new FSlateImageBrush(NoteIconPath, Icon40x40));
  Style->Set("NexxNote.NoteButton.Small",
             new FSlateImageBrush(NoteIconPath, Icon20x20));

  Style->Set("NexxNote.NoteIcon",
             new FSlateImageBrush(NoteIconPath, Icon40x40));
  Style->Set("NexxNote.NoteIcon.Small",
             new FSlateImageBrush(NoteIconPath, Icon20x20));

  // Renk değiştirici ikonu - özel boyut kullanılıyor
  Style->Set("NexxNote.ColorChangerButton",
             new FSlateImageBrush(ColorIconPath, ColorChangerIcon));
  Style->Set("NexxNote.ColorChangerButton.Small",
             new FSlateImageBrush(ColorIconPath, ColorChangerIcon));

  Style->Set("NexxNote.ColorChangerIcon",
             new FSlateImageBrush(ColorIconPath, ColorChangerIcon));
  Style->Set("NexxNote.ColorChangerIcon.Small",
             new FSlateImageBrush(ColorIconPath, ColorChangerIcon));

  // NoteFull PNG dosyası için tam yol
  FString PluginBaseDir =
      IPluginManager::Get().FindPlugin("NexxNote")->GetBaseDir();
  FString NoteFullFullPath =
      FPaths::Combine(PluginBaseDir, NoteFullPngPath + TEXT(".png"));

  // NoteFull PNG dosyasının varlığını kontrol et
  bool bNoteFullExists = IFileManager::Get().FileExists(*NoteFullFullPath);

  // NoteFull görüntüsünü tanımla
  Style->Set("NexxNote.NoteFull",
             new FSlateImageBrush(NoteFullFullPath, NoteFullSize));

  // NexxNote.Node görünümünü tanımla
  const FTextBlockStyle NormalText =
      FTextBlockStyle()
          .SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9))
          .SetColorAndOpacity(FSlateColor::UseForeground())
          .SetShadowOffset(FVector2D::ZeroVector)
          .SetShadowColorAndOpacity(FLinearColor::Black);

  Style->Set("NexxNote.TextStyle", NormalText);

  return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

const ISlateStyle &FNexxNoteStyle::Get() { return *StyleInstance.Get(); }
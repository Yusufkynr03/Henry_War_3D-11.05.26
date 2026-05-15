// Copyright 2025 Onur Altuntaş. All Rights Reserved.

#include "NexxNote.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "LevelEditor.h"
#include "NexxNoteCommands.h"
#include "NexxNoteHelpers.h"
#include "ToolMenus.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Styling/AppStyle.h"
#include "ToolMenuSection.h"

#include "BlueprintEditor.h"
#include "Containers/Array.h"
#include "EdGraphUtilities.h"
#include "Editor/UnrealEdEngine.h"
#include "EngineGlobals.h"
#include "HAL/Platform.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Math/Color.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "NexxNoteGraphNodeFactory.h"
#include "NexxNoteGraphPanelExtension.h"
#include "SGraphNodeSketch.h"
#include "SNexxNoteColorPicker.h"
#include "SketchNode.h"
#include "Templates/SharedPointer.h"
#include "UnrealEdGlobals.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

#define LOCTEXT_NAMESPACE "FNexxNoteModule"

// FindAllWidgetsRecursive artık NexxNoteHelpers.h dosyasında tanımlandı

// Static members
FName FNexxNoteModule::NexxNoteTabName(TEXT("NexxNote"));

void FNexxNoteModule::StartupModule() {
  // Initialize flags
  bSketchModeActive = false;
  bNoteModeActive = false;

  // İlk önce stil tanımlamalarını yükle
  FNexxNoteStyle::Initialize();

  // ÖNEMLİ: TCommands sınıflarında doğru yaklaşım budur
  // 1. Önce Register() çağrılmalı
  // 2. Sonra TCommands<>::Get() ile önceden oluşturulmuş aynı nesne alınmalı
  // Yoksa komutlar geçersiz olur
  FNexxNoteCommands::Register();

  // ÖNEMLİ: TCommands sınıfını Get() üzerinden kullanırken TSharedRef
  // veya doğrudan referans kullanmak gerekir, TSharedPtr değil
  // DÜZELTME: TSharedPtr -> referans şeklinde alıp kullanıyoruz
  const FNexxNoteCommands &CommandsRef = FNexxNoteCommands::Get();

  // Komutların geçerliliğini kontrol et
  bool bSketchCommandValid = CommandsRef.ToggleSketchMode.IsValid();
  bool bNoteCommandValid = CommandsRef.ToggleNoteMode.IsValid();
  bool bPluginActionValid = CommandsRef.PluginAction.IsValid();

  // Register node factory
  GraphNodeFactory = MakeShareable(new FNexxNoteGraphNodeFactory());
  FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeFactory);

  // Register tab spawner
  FGlobalTabmanager::Get()
      ->RegisterNomadTabSpawner(
          NexxNoteTabName,
          FOnSpawnTab::CreateRaw(this, &FNexxNoteModule::OnSpawnPluginTab))
      .SetDisplayName(NSLOCTEXT("FNexxNoteModule", "TabTitle", "NexxNote"))
      .SetMenuType(ETabSpawnerMenuType::Hidden);

  // ÖNEMLİ: UI Command listesini oluştur ve buton komutlarını bağla
  UICommandList = MakeShareable(new FUICommandList);

  // Artık TSharedPtr yerine referans olarak aldığımız komutları kullanıyoruz
  // Komutları FNexxNoteCommands::Get() üzerinden erişim sağlayarak bağlıyoruz
  if (UICommandList.IsValid()) {
    // Sketch modu için komut bağlama
    if (CommandsRef.ToggleSketchMode.IsValid()) {
      UICommandList->MapAction(
          CommandsRef.ToggleSketchMode,
          FExecuteAction::CreateRaw(this,
                                    &FNexxNoteModule::OnSketchButtonClicked),
          FCanExecuteAction(),
          FIsActionChecked::CreateRaw(this,
                                      &FNexxNoteModule::IsSketchModeActive));
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] ToggleSketchMode command is not valid!"));
    }

    // Not modu için komut bağlama
    if (CommandsRef.ToggleNoteMode.IsValid()) {
      UICommandList->MapAction(
          CommandsRef.ToggleNoteMode,
          FExecuteAction::CreateRaw(this,
                                    &FNexxNoteModule::OnNoteModeButtonClicked),
          FCanExecuteAction(),
          FIsActionChecked::CreateRaw(this,
                                      &FNexxNoteModule::IsNoteModeActive));
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] ToggleNoteMode command is not valid!"));
    }

    // Plugin action command list
    PluginCommands = MakeShareable(new FUICommandList);

    if (CommandsRef.PluginAction.IsValid()) {
      PluginCommands->MapAction(
          CommandsRef.PluginAction,
          FExecuteAction::CreateRaw(this,
                                    &FNexxNoteModule::PluginButtonClicked),
          FCanExecuteAction());
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] PluginAction command is not valid!"));
    }
  } else {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] UICommandList is not valid - skipping binding"));
  }

  // Motor hazır olduğunda menüleri kaydedecek
  FCoreDelegates::OnPostEngineInit.AddRaw(this,
                                          &FNexxNoteModule::OnPostEngineInit);

  RegisterBlueprintToolbarExtension();

  // Create graph panel extension
  GraphPanelExtension = MakeShareable(new FNexxNoteGraphPanelExtension());
  GraphPanelExtension->Register();

  // Setup callback for sketch mode status changes
  GraphPanelExtension->OnSketchModeStateChanged.BindLambda(
      [this](bool bNewState) {
        // If the GraphPanelExtension changes the state (like when
        // right-clicking), update our state
        if (bSketchModeActive != bNewState) {
          bSketchModeActive = bNewState;
          UpdateButtonStates();
        }
      });

  // Setup callback for note mode status changes
  GraphPanelExtension->OnNoteModeStateChanged.BindLambda(
      [this](bool bNewState) {
        // If the GraphPanelExtension changes the not mode state, update our
        // state
        if (bNoteModeActive != bNewState) {
          bNoteModeActive = bNewState;
          UpdateButtonStates();
        }
      });

  // Set initial button states
  UpdateButtonStates();
}

// Engine tamamen başladığında çağrılacak fonksiyon
void FNexxNoteModule::OnPostEngineInit() {
  // Engine başlatıldıktan sonra menüleri kaydet
  UE_LOG(LogTemp, Warning,
         TEXT("[NexxNote] Post engine init - registering menus"));
  RegisterMenus();

  // Tek seferlik delegate bağlantısını kaldır
  FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

void FNexxNoteModule::ShutdownModule() {
  // Unregister extensions when module shuts down
  UnregisterBlueprintToolbarExtension();

  // Unregister node factory
  if (GraphNodeFactory.IsValid()) {
    FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeFactory);
    GraphNodeFactory.Reset();
  }

  // Unregister graph panel extension
  if (GraphPanelExtension.IsValid()) {
    GraphPanelExtension->Unregister();
    GraphPanelExtension.Reset();
  }

  // Clean up ToolMenus
  if (UToolMenus::IsToolMenuUIEnabled()) {
    UToolMenus::Get()->UnregisterOwner(this);
  }

  // Unregister commands
  FNexxNoteCommands::Unregister();
}

void FNexxNoteModule::RegisterMenus() {
  // Öncelikle UToolMenus'ün tamamen başlatılıp başlatılmadığını kontrol edelim
  if (!UToolMenus::IsToolMenuUIEnabled()) {
    UE_LOG(LogTemp, Warning,
           TEXT("[NexxNote] ToolMenus is not initialized yet, skipping menu "
                "registration"));
    return;
  }

  // Güvenlik kontrolü - UToolMenus::Get() null dönebilir
  UToolMenus *ToolMenus = UToolMenus::Get();
  if (!ToolMenus) {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] UToolMenus::Get() returned nullptr, skipping menu "
                "registration"));
    return;
  }

  // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
  FToolMenuOwnerScoped OwnerScoped(this);

  // Menü yoksa atlayalım
  UToolMenu *Menu = ToolMenus->ExtendMenu("LevelEditor.MainMenu.Window");
  if (!Menu) {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] Could not find or extend "
                "LevelEditor.MainMenu.Window menu"));
    return;
  }

  FToolMenuSection &Section = Menu->FindOrAddSection("NexxNoteSection");

  // CommandsRef değişkenini burada yeniden tanımlayalım, çünkü her çağrıda
  // geçerli bir referans almak güvenlidir
  const FNexxNoteCommands &CommandsRef = FNexxNoteCommands::Get();

  // Komutların geçerliliğini kontrol et
  if (UICommandList.IsValid()) {
    // UICommandList kullanmız gerekiyor, PluginCommands değil
    UE_LOG(LogTemp, Warning,
           TEXT("[NexxNote] Adding menu entries with UICommandList"));
    Section.AddMenuEntryWithCommandList(CommandsRef.ToggleSketchMode,
                                        UICommandList);
    Section.AddMenuEntryWithCommandList(CommandsRef.ToggleNoteMode,
                                        UICommandList);
  } else {
    UE_LOG(
        LogTemp, Error,
        TEXT("[NexxNote] UICommandList is not valid, skipping menu entries"));
  }
}

void FNexxNoteModule::RegisterBlueprintToolbarExtension() {
  // Register toolbar extender for blueprint editor
  ToolbarExtender = MakeShareable(new FExtender);

  // Add extension to insert our buttons after the debugging section
  ToolbarExtender->AddToolBarExtension(
      "Debugging", EExtensionHook::After, UICommandList,
      FToolBarExtensionDelegate::CreateRaw(
          this, &FNexxNoteModule::AddToolbarButtons));

  // Get the blueprint editor module and add our extender
  FBlueprintEditorModule &BlueprintEditorModule =
      FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
  BlueprintEditorModule.GetMenuExtensibilityManager()->AddExtender(
      ToolbarExtender);
}

void FNexxNoteModule::UnregisterBlueprintToolbarExtension() {
  // Unregister our toolbar extension
  if (ToolbarExtender.IsValid()) {
    FBlueprintEditorModule &BlueprintEditorModule =
        FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
    BlueprintEditorModule.GetMenuExtensibilityManager()->RemoveExtender(
        ToolbarExtender);
    ToolbarExtender.Reset();
  }
}

void FNexxNoteModule::AddToolbarButtons(FToolBarBuilder &ToolbarBuilder) {
  // Güvenlik kontrolü - komutlar ve stil mevcut mu?
  if (!UICommandList.IsValid() || !FNexxNoteStyle::IsInitialized()) {
    return;
  }

  // AppStyle'a erişimin geçerli olduğundan emin ol
  const ISlateStyle *AppStylePtr = &FAppStyle::Get();
  if (!AppStylePtr) {
    return;
  }

  // CommandsRef değişkenini burada yeniden tanımlayalım
  const FNexxNoteCommands &CommandsRef = FNexxNoteCommands::Get();

  // Komutların geçerliliğini kontrol et
  bool bSketchCommandValid = CommandsRef.ToggleSketchMode.IsValid();
  bool bNoteCommandValid = CommandsRef.ToggleNoteMode.IsValid();

  // Komut düğmelerini ekleyin, ancak her aşamada güvenlik kontrolü yapın
  try {
    ToolbarBuilder.BeginSection("NexxNoteToolbar");
    {
      // Sketch butonunu ekle - basit FUICommandInfo kullanarak
      if (CommandsRef.ToggleSketchMode.IsValid()) {
        // Stil kaynağının geçerliliğini kontrol et
        const FName StyleSetName("NexxNoteStyle");
        // DÜZELTME: Stil adlarını NexxNoteStyle.cpp'de tanımlananlar ile
        // eşleştir
        const FName IconName("NexxNote.SketchButton");
        const FName SmallIconName("NexxNote.SketchButton.Small");

        // FSlateIcon nesnesini güvenli bir şekilde oluştur
        FSlateIcon SketchIcon(StyleSetName, IconName, SmallIconName);

        ToolbarBuilder.AddToolBarButton(
            CommandsRef.ToggleSketchMode, NAME_None,
            LOCTEXT("SketchButtonLabel", "Sketch"),
            LOCTEXT("SketchButtonTooltip",
                    "Allows you to draw on the Event Graph"),
            SketchIcon, FName("NexxNoteSketch"));
      }

      // Renk seçici butonu ekle - sadece temel style kullan, özel icon
      // kullanımını kaldır

      // Renk değiştirici ikonu için stil bilgilerini al
      // Daha önce tanımlandığı için farklı isim kullanalım
      const FName ColorStyleSetName("NexxNoteStyle");
      const FName ColorIconName("NexxNote.ColorChangerButton");
      const FName ColorSmallIconName("NexxNote.ColorChangerButton.Small");

      // İkonu oluştur
      FSlateIcon ColorIcon(ColorStyleSetName, ColorIconName,
                           ColorSmallIconName);

      // Renk seçici butonu da diğer butonlar gibi komut şeklinde ekle
      if (CommandsRef.ColorPickerAction.IsValid()) {
        ToolbarBuilder.AddToolBarButton(
            CommandsRef.ColorPickerAction, NAME_None,
            LOCTEXT("ColorPickerButtonLabel",
                    ""), // Etiket boş olsun, sadece ikon gösterilecek
            LOCTEXT("ColorPickerTooltip", "Drawing Color and Thickness"),
            ColorIcon, FName("NexxNoteColorPicker"));

        // ColorPickerAction komutunu UICommandList'e bağla
        UICommandList->MapAction(
            CommandsRef.ColorPickerAction,
            FExecuteAction::CreateLambda([this]() {
              // Slate uygulamasının başlatıldığından emin ol
              if (!FSlateApplication::IsInitialized()) {
                return;
              }

              // GraphPanelExtension geçerliliğini kontrol et
              if (!GraphPanelExtension.IsValid()) {
                return;
              }

              // Mevcut değerleri al
              FLinearColor CurrentColor =
                  GraphPanelExtension->GetCurrentDrawingColor();
              float CurrentThickness =
                  GraphPanelExtension->GetCurrentDrawingThickness();

              // Widget oluştur ve geçerliliğini kontrol et
              try {
                PersistentColorPickerWidget =
                    SNew(SNexxNoteColorPicker)
                        .InitialColor(CurrentColor)
                        .InitialThickness(CurrentThickness)
                        .OnColorSelected_Lambda([this](FLinearColor NewColor) {
                          if (GraphPanelExtension.IsValid()) {
                            GraphPanelExtension->SetDrawingColor(NewColor);
                          }
                        })
                        .OnThicknessSelected_Lambda([this](float NewThickness) {
                          if (GraphPanelExtension.IsValid()) {
                            GraphPanelExtension->SetDrawingThickness(
                                NewThickness);
                          }
                        });
              } catch (...) {
                return;
              }

              // Widget oluşturulduğundan emin ol
              if (!PersistentColorPickerWidget.IsValid()) {
                return;
              }

              // Aktif pencereyi bul
              TSharedPtr<SWindow> ActiveWindow =
                  FSlateApplication::Get().GetActiveTopLevelWindow();
              if (!ActiveWindow.IsValid()) {
                return;
              }

              // Menüyü göster
              try {
                FSlateApplication::Get().PushMenu(
                    ActiveWindow.ToSharedRef(), FWidgetPath(),
                    PersistentColorPickerWidget.ToSharedRef(),
                    FSlateApplication::Get().GetCursorPos(),
                    FPopupTransitionEffect(FPopupTransitionEffect::None),
                    false);
              } catch (...) {
                return;
              }
            }),
            FCanExecuteAction());
      } else {
        // Komut geçerli değilse widget yaklaşımına geri dön (sadece geçici
        // olarak)
        ToolbarBuilder.AddWidget(
            SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "ToolBar.Button")
                .ContentPadding(
                    FAppStyle::Get().GetMargin("ToolBar.Button.Normal.Padding"))
                .OnClicked_Lambda([this]() -> FReply {
                  // Slate uygulamasının başlatıldığından emin ol
                  if (!FSlateApplication::IsInitialized()) {
                    return FReply::Handled();
                  }

                  // GraphPanelExtension geçerliliğini kontrol et
                  if (!GraphPanelExtension.IsValid()) {
                    return FReply::Handled();
                  }

                  // Mevcut değerleri al
                  FLinearColor CurrentColor =
                      GraphPanelExtension->GetCurrentDrawingColor();
                  float CurrentThickness =
                      GraphPanelExtension->GetCurrentDrawingThickness();

                  // Widget oluştur ve geçerliliğini kontrol et
                  try {
                    PersistentColorPickerWidget =
                        SNew(SNexxNoteColorPicker)
                            .InitialColor(CurrentColor)
                            .InitialThickness(CurrentThickness)
                            .OnColorSelected_Lambda([this](
                                                        FLinearColor NewColor) {
                              if (GraphPanelExtension.IsValid()) {
                                GraphPanelExtension->SetDrawingColor(NewColor);
                              }
                            })
                            .OnThicknessSelected_Lambda(
                                [this](float NewThickness) {
                                  if (GraphPanelExtension.IsValid()) {
                                    GraphPanelExtension->SetDrawingThickness(
                                        NewThickness);
                                  }
                                });
                  } catch (...) {
                    return FReply::Handled();
                  }

                  // Widget oluşturulduğundan emin ol
                  if (!PersistentColorPickerWidget.IsValid()) {
                    return FReply::Handled();
                  }

                  // Aktif pencereyi bul
                  TSharedPtr<SWindow> ActiveWindow =
                      FSlateApplication::Get().GetActiveTopLevelWindow();
                  if (!ActiveWindow.IsValid()) {
                    return FReply::Handled();
                  }

                  // Menüyü göster
                  try {
                    FSlateApplication::Get().PushMenu(
                        ActiveWindow.ToSharedRef(), FWidgetPath(),
                        PersistentColorPickerWidget.ToSharedRef(),
                        FSlateApplication::Get().GetCursorPos(),
                        FPopupTransitionEffect(FPopupTransitionEffect::None),
                        false);
                  } catch (...) {
                    return FReply::Handled();
                  }

                  return FReply::Handled();
                })
                .ToolTipText(LOCTEXT("ColorPickerTooltip",
                                     "Drawing Color and Thickness"))
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                .Content()
                    [SNew(SBox)
                         .WidthOverride(20.0f)  // Boyutu büyüttüm ortalama için
                         .HeightOverride(20.0f) // Boyutu büyüttüm ortalama için
                         .HAlign(HAlign_Center)
                         .VAlign(VAlign_Center)
                         .Padding(FMargin(0.0f)) // Padding'i sıfırla
                             [SNew(SImage)
                                  .Image(FNexxNoteStyle::Get().GetBrush(
                                      ColorIconName))
                                  .ColorAndOpacity(
                                      FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))]]);
      }

      // Not butonunu ekle - basit FUICommandInfo kullanarak
      if (CommandsRef.ToggleNoteMode.IsValid()) {
        // Stil kaynağının geçerliliğini kontrol et
        const FName StyleSetName("NexxNoteStyle");
        // DÜZELTME: Stil adlarını NexxNoteStyle.cpp'de tanımlananlar ile
        // eşleştir
        const FName IconName("NexxNote.NoteButton");
        const FName SmallIconName("NexxNote.NoteButton.Small");

        // FSlateIcon nesnesini güvenli bir şekilde oluştur
        FSlateIcon NoteIcon(StyleSetName, IconName, SmallIconName);

        ToolbarBuilder.AddToolBarButton(
            CommandsRef.ToggleNoteMode, NAME_None,
            LOCTEXT("NoteButtonLabel", "Note"),
            LOCTEXT("NoteButtonTooltip",
                    "Allows you to add notes to the Event Graph"),
            NoteIcon, FName("NexxNoteAddNote"));
      }
    }
    ToolbarBuilder.EndSection();
  } catch (...) {
    // Unknown exception in AddToolbarButtons
  }
}

bool FNexxNoteModule::IsSketchModeActive() const { return bSketchModeActive; }

bool FNexxNoteModule::IsNoteModeActive() const { return bNoteModeActive; }

void FNexxNoteModule::OnSketchButtonClicked() {
  // GÜVENLIK: Durum değişikliğini hemen kaydet
  bool bWasSketchModeActive = bSketchModeActive;

  // Sketch modu durumunu tersine çevir
  bSketchModeActive = !bSketchModeActive;

  // Not modunu devre dışı bırak
  bNoteModeActive = false;

  // GÜVENLIK: Slate uygulaması başlatılmış mı kontrol et
  if (!FSlateApplication::IsInitialized()) {
    UE_LOG(
        LogTemp, Error,
        TEXT(
            "[NexxNote] FSlateApplication not initialized, operation skipped"));
    // Durumu güvenli değere geri al
    bSketchModeActive = false;
    UpdateButtonStates();
    return;
  }

  // GÜVENLIK: Input temizleme işlemleri
  try {
    FSlateApplication::Get().ReleaseAllPointerCapture();
    FSlateApplication::Get().ResetToDefaultInputSettings();
  } catch (...) {
    UE_LOG(LogTemp, Error, TEXT("[NexxNote] Error during input cleanup"));
  }

  // GÜVENLIK: GraphPanelExtension'ın geçerliliğini kontrol et
  if (!GraphPanelExtension.IsValid()) {
    UE_LOG(LogTemp, Warning,
           TEXT("[NexxNote] OnSketchButtonClicked: GraphPanelExtension "
                "invalid, recreating"));

    // Extension'ı yeniden oluştur
    try {
      GraphPanelExtension = MakeShareable(new FNexxNoteGraphPanelExtension());
      GraphPanelExtension->Register();

      // Sketch modu değişiklik olayını dinle
      GraphPanelExtension->OnSketchModeStateChanged.BindLambda(
          [this](bool bNewState) {
            if (bSketchModeActive != bNewState) {
              bSketchModeActive = bNewState;
              UpdateButtonStates();
            }
          });
    } catch (...) {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] Failed to create new GraphPanelExtension"));
      // Durumu güvenli değere geri al
      bSketchModeActive = false;
      UpdateButtonStates();
      return;
    }
  }

  // GÜVENLIK: GraphPanelExtension hala geçerli mi kontrol et
  if (!GraphPanelExtension.IsValid()) {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] Still unable to create GraphPanelExtension, "
                "operation cancelled"));
    // Durumu güvenli değere geri al
    bSketchModeActive = false;
    UpdateButtonStates();
    return;
  }

  // Sketch modunu kapatırken, önce overlay'leri temizle
  if (!bSketchModeActive && bWasSketchModeActive) {
    // Eğer GraphPanelExtension mevcutsa, overlay'leri tamamen temizle
    if (GraphPanelExtension.IsValid()) {
      try {
        GraphPanelExtension->ClearAllOverlays();
      } catch (...) {
        UE_LOG(LogTemp, Error,
               TEXT("[NexxNote] Error occurred during ClearAllOverlays call"));
      }
    }
  }

  // Update graph panel extension - Önemli: Burada önce GraphPanelExtension'a
  // yeni durumu bildirelim
  if (GraphPanelExtension.IsValid()) {
    // ÖNEMLİ: Sketch modu aktifleştirmeden ÖNCE mevcut tüm blueprint editörleri
    // için overlay'leri güncelle Bu, ilk tıklamada çizim modunun doğru şekilde
    // çalışmasını sağlar
    if (bSketchModeActive) {
      try {
        // Temiz başla - mevcut overlay'leri kaldır
        GraphPanelExtension->ClearAllOverlays();

        // Yeni editörleri bul
        GraphPanelExtension->CheckForBlueprintEditors();
      } catch (...) {
        UE_LOG(LogTemp, Error, TEXT("[NexxNote] Error during editor setup"));
        // Durumu güvenli değere geri al
        bSketchModeActive = false;
        UpdateButtonStates();
        return;
      }
    }

    // Sketch modunu ayarla - artık tüm overlay'ler ayarlanmış durumda
    try {
      GraphPanelExtension->SetSketchModeActive(bSketchModeActive);

      // ÖNEMLİ: Eğer sketch modunu kapatıyorsak, tüm overlay'leri temizle
      if (!bSketchModeActive && bWasSketchModeActive) {
        // Eğer GraphPanelExtension mevcutsa, overlay'leri tamamen temizle
        if (GraphPanelExtension.IsValid()) {
          try {
            GraphPanelExtension->ClearAllOverlays();
          } catch (...) {
            UE_LOG(
                LogTemp, Error,
                TEXT("[NexxNote] Error occurred during ClearAllOverlays call"));
          }
        }
      }

      GraphPanelExtension->SetNoteModeActive(bNoteModeActive);
    } catch (...) {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] Unknown error in SetSketchModeActive"));
      // Durumu güvenli değere geri al
      bSketchModeActive = false;
    }

    // Değişiklikler sonrası bir kez daha yenile
    if (bSketchModeActive) {
      try {
        GraphPanelExtension->CheckForBlueprintEditors();
      } catch (...) {
        UE_LOG(LogTemp, Error,
               TEXT("[NexxNote] Error during post-change editor update"));
      }
    }
  } else {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] GraphPanelExtension is not valid! Recreating "
                "extension..."));

    // Eğer GraphPanelExtension henüz oluşturulmadıysa veya geçersizse, oluştur
    // ve kaydet
    try {
      GraphPanelExtension = MakeShareable(new FNexxNoteGraphPanelExtension());
      if (!GraphPanelExtension.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("[NexxNote] Failed to recreate extension"));
        // Durumu güvenli değere geri al
        bSketchModeActive = false;
        UpdateButtonStates();
        return;
      }

      GraphPanelExtension->Register();

      // Sketch modunun değişiklik olayını dinle
      GraphPanelExtension->OnSketchModeStateChanged.BindLambda(
          [this](bool bNewState) {
            if (bSketchModeActive != bNewState) {
              bSketchModeActive = bNewState;
              UpdateButtonStates();
            }
          });

      // Mod ayarlarını güncelle
      GraphPanelExtension->SetSketchModeActive(bSketchModeActive);
      GraphPanelExtension->SetNoteModeActive(bNoteModeActive);
    } catch (...) {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] Exception when recreating extension"));
      // Durumu güvenli değere geri al
      bSketchModeActive = false;
    }
  }

  // Butonların görsel durumunu güncelle
  UpdateButtonStates();
}

void FNexxNoteModule::OnNoteModeButtonClicked() {
  // GÜVENLIK: Durumu hemen güncelle
  bool bPreviousSketchModeActive = bSketchModeActive;
  bNoteModeActive = true;    // Not modunu aktif hale getir
  bSketchModeActive = false; // Çizim modunu devre dışı bırak

  // GÜVENLIK: Slate uygulaması kontrol et
  if (!FSlateApplication::IsInitialized()) {
    UE_LOG(
        LogTemp, Error,
        TEXT(
            "[NexxNote] FSlateApplication not initialized, operation skipped"));
    bNoteModeActive = false;
    UpdateButtonStates();
    return;
  }

  // GÜVENLIK: Input cleanup - herhangi bir fare yakalamayı serbest bırak
  try {
    FSlateApplication::Get().ReleaseAllPointerCapture();
    FSlateApplication::Get().ResetToDefaultInputSettings();
  } catch (...) {
    UE_LOG(LogTemp, Error, TEXT("[NexxNote] Error during input cleanup"));
  }

  // GÜVENLIK: Extension kontrolü ve gerekirse yeniden oluşturma
  bool bExtensionValid = GraphPanelExtension.IsValid();
  bool bExtensionRecreated = false;

  if (!bExtensionValid) {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] GraphPanelExtension invalid, recreating"));

    try {
      GraphPanelExtension = MakeShareable(new FNexxNoteGraphPanelExtension());
      if (GraphPanelExtension.IsValid()) {
        GraphPanelExtension->Register();

        // Olay dinleyicileri
        GraphPanelExtension->OnSketchModeStateChanged.BindLambda(
            [this](bool bNewState) {
              if (bSketchModeActive != bNewState) {
                bSketchModeActive = bNewState;
                UpdateButtonStates();
              }
            });

        bExtensionRecreated = true;
      } else {
        UE_LOG(LogTemp, Error,
               TEXT("[NexxNote] GraphPanelExtension could not be recreated"));
        GraphPanelExtension.Reset();
      }
    } catch (...) {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] Error creating GraphPanelExtension"));
      GraphPanelExtension.Reset();
    }
  }

  // GÜVENLIK: Extension'ın geçerli olup olmadığını yeniden kontrol et
  if (!GraphPanelExtension.IsValid()) {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] GraphPanelExtension still invalid, operation "
                "cancelled"));
    bNoteModeActive = false;
    UpdateButtonStates();
    return;
  }

  // Daha güvenli Extension işlemi
  bool bSuccess = false;
  try {
    // GÜVENLIK: Sketch modu devre dışı - bu çökme noktalarından biri
    try {
      GraphPanelExtension->SetSketchModeActive(false);
    } catch (...) {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] Error while disabling sketch mode"));
      // Ciddi bir hata, ancak devam edelim
    }

    // Blueprint editörleri kontrol et
    try {
      GraphPanelExtension->CheckForBlueprintEditors();
    } catch (...) {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] Error during blueprint editor check"));
      // Not kritik, devam et
    }

    // Not modunu aktifleştir - bu da bir çökme noktası
    try {
      GraphPanelExtension->SetNoteModeActive(true);
      bSuccess = true;
    } catch (...) {
      UE_LOG(LogTemp, Error,
             TEXT("[NexxNote] Error during note mode activation"));
      // Başarısız, işaretle
      bSuccess = false;
    }
  } catch (...) {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] General error during note operation"));
    bSuccess = false;
  }

  // İşlem sonuçlarına göre durumu güncelle
  if (!bSuccess) {
    UE_LOG(LogTemp, Error,
           TEXT("[NexxNote] Note operation failed, resetting states"));
    bNoteModeActive = false;

    // Eğer önceden sketch modu aktifse, tekrar aktifleştirmeyi dene
    if (bPreviousSketchModeActive && GraphPanelExtension.IsValid()) {
      try {
        GraphPanelExtension->SetSketchModeActive(true);
        bSketchModeActive = true;
      } catch (...) {
        UE_LOG(LogTemp, Error,
               TEXT("[NexxNote] Error reactivating sketch mode"));
      }
    }
  } else {
    // İşlem başarılı - Note modu bir kerelik operasyon, hemen devre dışı bırak
    bNoteModeActive = false;
  }

  // Son olarak buton durumlarını güncelle
  UpdateButtonStates();
}

void FNexxNoteModule::UpdateButtonStates() {
  // Not: FUICommandInfo'nın SetCheckState metodu olmadığı için burada yapacak
  // bir şey yok Button durumlarını FExecuteAction ve FIsActionChecked
  // lambda'ları ile zaten belirledik Ve lambda fonksiyonunda bSketchModeActive
  // ve bNoteModeActive değişkenlerini kullandığımız için buton durumları
  // otomatik olarak güncellenecektir
}

TSharedRef<SDockTab>
FNexxNoteModule::OnSpawnPluginTab(const FSpawnTabArgs &SpawnTabArgs) {
  return SNew(SDockTab).TabRole(ETabRole::NomadTab)
      [SNew(SBox)
           .HAlign(HAlign_Center)
           .VAlign(VAlign_Center)[SNew(STextBlock)
                                      .Text(NSLOCTEXT("NexxNoteTab", "TabTitle",
                                                      "NexxNote Plugin Tab"))]];
}

void FNexxNoteModule::PluginButtonClicked() {
  FGlobalTabmanager::Get()->TryInvokeTab(NexxNoteTabName);
}

// FindAllWidgetsRecursive fonksiyonu artık NexxNoteHelpers.h'den kullanılıyor

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNexxNoteModule, NexxNote)
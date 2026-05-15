// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "NexxNoteGraphPanelExtension.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintEditor.h"
#include "Containers/Ticker.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditorModule.h"
#include "GraphEditorSettings.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Layout/WidgetPath.h"
#include "Misc/CoreDelegates.h"
#include "NexxNote.h"
#include "NexxNoteHelpers.h"
#include "SGraphPanel.h"
#include "SNexxNoteDrawingOverlay.h"
#include "ScopedTransaction.h"
#include "SketchNode.h"
#include "UNexxNoteNode.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "NexxNoteGraphPanelExtension"

// FindAllWidgetsRecursive artık NexxNoteHelpers.h dosyasında tanımlandı

// Input processor'larını takip etmek için
static TArray<TSharedPtr<FNexxNoteInputProcessor>> RegisteredInputProcessors;

FNexxNoteGraphPanelExtension::FNexxNoteGraphPanelExtension()
    : bSketchModeActive(false), bNoteModeActive(false),
      SavedDrawingColor(FLinearColor::Yellow), SavedDrawingThickness(3.0f) {}

FNexxNoteGraphPanelExtension::~FNexxNoteGraphPanelExtension() { Unregister(); }

void FNexxNoteGraphPanelExtension::Register() {
  // PostEngineInit'e bağlan
  FCoreDelegates::OnPostEngineInit.AddRaw(
      this, &FNexxNoteGraphPanelExtension::OnPostEngineInit);
}

void FNexxNoteGraphPanelExtension::OnPostEngineInit() {
  // FTSTicker'a bağlan (bool dönüş türü beklenir)
  TickDelegate = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateRaw(this, &FNexxNoteGraphPanelExtension::Tick));

  // İlk kontrol
  CheckForBlueprintEditors();
}

void FNexxNoteGraphPanelExtension::Unregister() {
  // Tüm input processors'ları güvenli bir şekilde kaldır
  if (RegisteredInputProcessors.Num() > 0 &&
      FSlateApplication::IsInitialized()) {
    try {
      for (TSharedPtr<FNexxNoteInputProcessor> &Processor :
           RegisteredInputProcessors) {
        if (Processor.IsValid()) {
          // Önce imleci sıfırla
          try {
            Processor->ResetCursor();
          } catch (...) {
            // Error handling silently
          }

          // Sonra Slate uygulamasından kaldır
          try {
            FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
          } catch (...) {
            // Error handling silently
          }
        }
      }
    } catch (...) {
      // Error handling silently
    }

    // Listeyi temizle
    RegisteredInputProcessors.Empty();
  }

  // PostEngineInit delegate bağlantısını kaldır
  try {
    FCoreDelegates::OnPostEngineInit.RemoveAll(this);
  } catch (...) {
    // Error handling silently
  }

  // ClearAllOverlays'i güvenli bir şekilde çağır
  try {
    ClearAllOverlays(); // Ensure overlays are cleared on unregister
  } catch (const std::exception &) {
    // Hata durumunda haritaları manuel temizle
    GraphPanelToOverlayMap.Empty();
    GraphPanelToSketchWidgetMap.Empty();
  } catch (...) {
    // Hata durumunda haritaları manuel temizle
    GraphPanelToOverlayMap.Empty();
    GraphPanelToSketchWidgetMap.Empty();
  }
}

bool FNexxNoteGraphPanelExtension::Tick(float DeltaTime) {
  // Her 1 saniyede bir kontrol et
  static float TimeSinceLastCheck = 0.0f;
  TimeSinceLastCheck += DeltaTime;

  if (TimeSinceLastCheck >= 1.0f) {
    CheckForBlueprintEditors();
    TimeSinceLastCheck = 0.0f;
  }
  return true; // Ticker'ın devam etmesi için true döndür
}

void FNexxNoteGraphPanelExtension::CheckForBlueprintEditors() {
  // Uygulama hazır mı kontrol et
  if (!FSlateApplication::IsInitialized()) {
    return;
  }

  // Aktif pencereyi bul
  TSharedPtr<SWindow> ActiveWindow =
      FSlateApplication::Get().GetActiveTopLevelWindow();
  if (!ActiveWindow.IsValid()) {
    return;
  }

  // Tüm widget'ları topla
  TArray<TSharedRef<SWidget>> AllWidgets;
  try {
    NexxNoteHelpers::FindAllWidgetsRecursive(ActiveWindow.ToSharedRef(),
                                             AllWidgets);
  } catch (...) {
    return;
  }

  if (AllWidgets.Num() == 0) {
    return;
  }

  // SGraphPanel'leri bul
  TArray<SGraphPanel *> NewPanels;

  for (TSharedRef<SWidget> Widget : AllWidgets) {
    // TSharedRef her zaman geçerlidir, bu kontrole gerek yok

    if (Widget->GetTypeAsString() == TEXT("SGraphPanel")) {
      SGraphPanel *GraphPanel = static_cast<SGraphPanel *>(&Widget.Get());
      if (GraphPanel && !this->GraphPanelToOverlayMap.Contains(GraphPanel)) {
        // Panel geçerli mi kontrol et
        if (!GraphPanel->GetGraphObj()) {
          continue;
        }

        // Bu panel bizim için yeni, işle
        HandleNewGraphPanel(GraphPanel);
        NewPanels.Add(GraphPanel);
      }
    }
  }
}

void FNexxNoteGraphPanelExtension::HandleNewGraphPanel(
    SGraphPanel *GraphPanel) {
  // GraphPanel null kontrolü ekle
  if (!GraphPanel) {
    return;
  }

  // Sadece Blueprint Event Graph'ları için işlem yap
  const UEdGraphSchema *Schema = GraphPanel->GetGraphObj()
                                     ? GraphPanel->GetGraphObj()->GetSchema()
                                     : nullptr;
  if (!Schema || !Schema->IsA<UEdGraphSchema_K2>()) {
    return;
  }

  // Zaten işlenmiş mi kontrol et
  if (this->GraphPanelToOverlayMap.Contains(GraphPanel)) {
    return;
  }

  // Bu panel için bir DrawingOverlay oluştur
  TSharedPtr<SNexxNoteDrawingOverlay> DrawingOverlay =
      SNew(SNexxNoteDrawingOverlay, GraphPanel);

  // Sketch modu deaktive olduğunda bildir
  DrawingOverlay->OnSketchModeDeactivated.BindRaw(
      this, &FNexxNoteGraphPanelExtension::OnSketchModeDeactivated);

  // ÖNEMLİ: Önceden kaydedilmiş renk ve kalınlık değerlerini ayarla
  DrawingOverlay->SetDrawingColor(this->SavedDrawingColor);
  DrawingOverlay->SetDrawingThickness(this->SavedDrawingThickness);

  // Haritaya ekle
  this->GraphPanelToOverlayMap.Add(GraphPanel, DrawingOverlay);
  this->GraphPanelToSketchWidgetMap.Add(GraphPanel, DrawingOverlay);

  // ÖNEMLİ: Overlay'i GraphPanel'e doğrudan Child olarak ekleyelim, böylece
  // görünür olacak
  TSharedPtr<SOverlay> ParentOverlay =
      StaticCastSharedPtr<SOverlay>(GraphPanel->GetParentWidget());
  if (ParentOverlay.IsValid()) {
    // Eklemeden önce overlay'in geçerliliğini kontrol et
    if (DrawingOverlay.IsValid()) {
      try {
        // Slot ekleme işlemini try-catch ile güvenli hale getir
        ParentOverlay->AddSlot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)[DrawingOverlay.ToSharedRef()];
      } catch (...) {
        // Error handling silently
      }
    }
  } else {
    // Alternatif yaklaşım: Input processor kullan
    // Overlay'i parent'a eklemek için bir method bulalım
    // Bu event capture'ı doğrudan pencere seviyesine çıkarabilir
    TSharedPtr<FNexxNoteInputProcessor> InputProcessor =
        MakeShareable(new FNexxNoteInputProcessor(GraphPanel, DrawingOverlay));
    FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
    RegisteredInputProcessors.Add(InputProcessor);
  }

  try {
    // Panel olaylarını yönet - bunu daha doğrudan yapalım
    // Doğrudan capture delegate'ini override edelim - bu daha güçlü capture
    // sağlar
    GraphPanel->SetOnMouseButtonDown(FPointerEventHandler::CreateLambda(
        [this, GraphPanel](const FGeometry &MyGeometry,
                           const FPointerEvent &MouseEvent) -> FReply {
          // GraphPanel hala geçerli mi kontrol et
          if (!GraphPanel) {
            return FReply::Unhandled();
          }

          // Overlay'i güvenli bir şekilde bul
          TSharedPtr<SNexxNoteDrawingOverlay> Overlay;
          if (this->GraphPanelToOverlayMap.Contains(GraphPanel)) {
            Overlay = this->GraphPanelToOverlayMap.FindRef(GraphPanel);
          }

          if (Overlay.IsValid() && Overlay->IsSketchModeActive()) {
            // ÖNEMLİ: Panel içinde olup olmadığını kontrol et
            FVector2D LocalPos =
                MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            FVector2D PanelSize = MyGeometry.GetLocalSize();

            if (LocalPos.X >= 0 && LocalPos.Y >= 0 &&
                LocalPos.X <= PanelSize.X && LocalPos.Y <= PanelSize.Y) {
              // Doğru imleci ayarla
              GraphPanel->SetCursor(EMouseCursor::Crosshairs);

              // Event'i doğrudan işle ve CaptureMouse'u yalnızca panel
              // içindeyse deneyelim
              FReply Reply = Overlay->OnMouseButtonDown(MyGeometry, MouseEvent);

              // Fare capture'ını sadece panel içindeyken zorla
              if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
                if (Overlay.IsValid()) {
                  return FReply::Handled()
                      .CaptureMouse(Overlay.ToSharedRef())
                      .UseHighPrecisionMouseMovement(Overlay.ToSharedRef());
                } else {
                  return FReply::Handled();
                }
              }

              return Reply;
            } else {
              // Panel dışındayken normal imleç
              GraphPanel->SetCursor(EMouseCursor::Default);

              // Platform imlecini de sıfırla
              TSharedPtr<ICursor> PlatformCursor =
                  FSlateApplication::Get().GetPlatformCursor();
              if (PlatformCursor.IsValid()) {
                PlatformCursor->SetType(EMouseCursor::Default);
              }

              // Panel dışındayken fare olayını ele alma, böylece normal
              // butonlar çalışır
              return FReply::Unhandled();
            }
          }

          return FReply::Unhandled();
        }));

    GraphPanel->SetOnMouseButtonUp(FPointerEventHandler::CreateLambda(
        [this, GraphPanel](const FGeometry &MyGeometry,
                           const FPointerEvent &MouseEvent) -> FReply {
          // GraphPanel geçerlilik kontrolü
          if (!GraphPanel) {
            return FReply::Unhandled();
          }

          // Overlay'i güvenli bir şekilde bul
          TSharedPtr<SNexxNoteDrawingOverlay> Overlay;
          if (this->GraphPanelToOverlayMap.Contains(GraphPanel)) {
            Overlay = this->GraphPanelToOverlayMap.FindRef(GraphPanel);
          }

          if (Overlay.IsValid() && Overlay->IsSketchModeActive()) {
            // Panel içinde olup olmadığını kontrol et
            FVector2D LocalPos =
                MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            FVector2D PanelSize = MyGeometry.GetLocalSize();
            bool bIsInsidePanel =
                (LocalPos.X >= 0 && LocalPos.Y >= 0 &&
                 LocalPos.X <= PanelSize.X && LocalPos.Y <= PanelSize.Y);

            // Overlay'e durumu bildir - panel içindeyse normal çizimi sonlandır
            if (bIsInsidePanel) {
              FReply Reply = Overlay->OnMouseButtonUp(MyGeometry, MouseEvent);
            } else {
              // Panel dışındayken normal imleç göster
              GraphPanel->SetCursor(EMouseCursor::Default);

              // Platform imlecini de sıfırla
              TSharedPtr<ICursor> PlatformCursor =
                  FSlateApplication::Get().GetPlatformCursor();
              if (PlatformCursor.IsValid()) {
                PlatformCursor->SetType(EMouseCursor::Default);
              }
            }

            // ÖNEMLİ: Fare yakalamayı her durumda bırak
            if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
              return FReply::Handled().ReleaseMouseCapture();
            }

            return FReply::Handled();
          }
          return FReply::Unhandled();
        }));

    GraphPanel->SetOnMouseMove(FPointerEventHandler::CreateLambda(
        [this, GraphPanel](const FGeometry &MyGeometry,
                           const FPointerEvent &MouseEvent) -> FReply {
          // GraphPanel geçerlilik kontrolü
          if (!GraphPanel) {
            return FReply::Unhandled();
          }

          // Overlay'i güvenli bir şekilde bul
          TSharedPtr<SNexxNoteDrawingOverlay> Overlay;
          if (this->GraphPanelToOverlayMap.Contains(GraphPanel)) {
            Overlay = this->GraphPanelToOverlayMap.FindRef(GraphPanel);
          }

          if (Overlay.IsValid() && Overlay->IsSketchModeActive()) {
            // Panel içinde veya dışında olma durumunu kontrol et
            FVector2D LocalPos =
                MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            FVector2D PanelSize = MyGeometry.GetLocalSize();
            bool bIsInsidePanel =
                (LocalPos.X >= 0 && LocalPos.Y >= 0 &&
                 LocalPos.X <= PanelSize.X && LocalPos.Y <= PanelSize.Y);

            // İmleci panel içi/dışına göre ayarla
            if (bIsInsidePanel) {
              // Panel içindeyken çizim imleci
              GraphPanel->SetCursor(EMouseCursor::Crosshairs);

              // Fare yakalanmışsa çizim işlemlerini ilet
              if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) &&
                  FSlateApplication::Get().HasAnyMouseCaptor()) {
                return Overlay->OnMouseMove(MyGeometry, MouseEvent);
              }
            } else {
              // Panel dışındayken normal imleç
              GraphPanel->SetCursor(EMouseCursor::Default);

              // Platform imlecini de direkt değiştirelim
              TSharedPtr<ICursor> PlatformCursor =
                  FSlateApplication::Get().GetPlatformCursor();
              if (PlatformCursor.IsValid()) {
                PlatformCursor->SetType(EMouseCursor::Default);
              }
            }

            return FReply::Handled();
          }
          return FReply::Unhandled();
        }));
  } catch (const std::exception &) {
    // Error handling silently
  }
}

void FNexxNoteGraphPanelExtension::SetSketchModeActive(bool bActive) {
  // GÜVENLIK: Fonksiyonun başlangıcında durumu güncelle, böylece hata olsa bile
  // durum güncellenmiş olur
  this->bSketchModeActive = bActive;

  // 1. GÜVENLİK KATMANı - Slate başlatılmazsa çok erken çık
  if (!FSlateApplication::IsInitialized()) {
    // Yine de durumu bildiriyoruz
    this->OnSketchModeStateChanged.ExecuteIfBound(bActive);
    return;
  }

  // 2. GÜVENLİK KATMANI - Input processor temizliği
  if (!bActive) {
    // Önce global temizlik - tüm fare/klavye yakalamalarını bırak
    try {
      // Slate uygulaması üzerinde temizlik
      FSlateApplication::Get().ReleaseAllPointerCapture();
      FSlateApplication::Get().ResetToDefaultInputSettings();
      FSlateApplication::Get().ResetToDefaultPointerInputSettings();

      // Platform imlecini sıfırla
      TSharedPtr<ICursor> PlatformCursor =
          FSlateApplication::Get().GetPlatformCursor();
      if (PlatformCursor.IsValid()) {
        PlatformCursor->SetType(EMouseCursor::Default);
      }

      // Input processor'ları temizle
      for (auto &Processor : RegisteredInputProcessors) {
        if (Processor.IsValid()) {
          Processor->SetSketchModeActive(false);
          Processor->CompleteReset();
        }
      }
    } catch (...) {
      // Error handling silently
    }
  }

  // 3. GÜVENLİK KATMANI - Blueprint editörlerini tara ve haritayı güncelle
  if (bActive && GraphPanelToOverlayMap.Num() == 0) {
    // Editör taraması sırasında da koruma
    try {
      CheckForBlueprintEditors();
    } catch (...) {
      // Hata durumunda callback çağır ve çık
      this->OnSketchModeStateChanged.ExecuteIfBound(bActive);
      return;
    }

    // Hala harita boşsa - yapacak bir şey yok
    if (GraphPanelToOverlayMap.Num() == 0) {
      this->OnSketchModeStateChanged.ExecuteIfBound(bActive);
      return;
    }
  }

  // 4. GÜVENLİK KATMANI - İşlemi önce yerel değişkenlerde yap, sonra haritayı
  // güncelle
  TArray<SGraphPanel *> InvalidPanels;
  bool bAnyOverlayUpdated = false;

  // Haritanın kopyasını al - işlem sırasında haritayı değiştirmeden
  TMap<SGraphPanel *, TSharedPtr<SNexxNoteDrawingOverlay>> CopyMap;

  // Güvenli bir şekilde kopyala - null verileri dahil etme
  for (auto &Pair : GraphPanelToOverlayMap) {
    if (Pair.Key != nullptr && Pair.Value.IsValid()) {
      CopyMap.Add(Pair.Key, Pair.Value);
    } else {
      if (Pair.Key) {
        InvalidPanels.Add(Pair.Key);
      }
    }
  }

  // 5. GÜVENLİK KATMANI - Her panel için ayrı try/catch bloğu
  for (auto &Pair : CopyMap) {
    SGraphPanel *Panel = Pair.Key;
    TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;

    // Sadece geçerli panel ve overlay için işlem yap
    if (!Panel) {
      InvalidPanels.Add(Panel);
      continue;
    }

    // Overlay geçerli mi kontrol et
    if (!Overlay.IsValid()) {
      InvalidPanels.Add(Panel);
      continue;
    }

    // Graph nesnesini kontrol et
    UEdGraph *Graph = nullptr;
    try {
      // Önce pointer'ı kontrol et, sonra erişmeyi dene
      Graph = Panel->GetGraphObj();
    } catch (...) {
      InvalidPanels.Add(Panel);
      continue;
    }

    if (!Graph) {
      InvalidPanels.Add(Panel);
      continue;
    }

    // 6. GÜVENLİK KATMANI - Panel başına ayrı try/catch
    try {
      // GÜVENLIK: SetSketchModeActive önce ve ayrı bir blokta çağırılıyor
      bool bOverlayResult = false;
      try {
        // Bu, çökmeye neden olan kritik çağrı
        Overlay->SetSketchModeActive(bActive);
        bOverlayResult = true;
      } catch (...) {
        // Overlay çağrısı başarısız oldu, bu panel geçersiz
        InvalidPanels.Add(Panel);
        continue;
      }

      // Overlay ayarlanması başarılıysa, panel'i güncelle
      if (bOverlayResult) {
        // İmleç ayarla
        Panel->SetCursor(bActive ? EMouseCursor::Crosshairs
                                 : EMouseCursor::Default);

        // Panel yenileme
        Panel->Invalidate(EInvalidateWidgetReason::Layout);
        Panel->Invalidate(EInvalidateWidgetReason::Paint);

        bAnyOverlayUpdated = true;

        // Pencere yenileme (ayrı try-catch'de)
        try {
          TSharedPtr<SWindow> ParentWindow =
              FSlateApplication::Get().FindWidgetWindow(Panel->AsShared());
          if (ParentWindow.IsValid()) {
            ParentWindow->Invalidate(EInvalidateWidgetReason::Paint);
            ParentWindow->Invalidate(EInvalidateWidgetReason::Layout);
          }
        } catch (...) {
          // Bu kritik bir hata değil, devam ediyoruz
        }
      }
    } catch (...) {
      // General error in panel update
      InvalidPanels.Add(Panel);
    }
  }

  // 7. GÜVENLİK KATMANI - Geçersiz panelleri güvenli bir şekilde temizle
  if (InvalidPanels.Num() > 0) {
    for (SGraphPanel *InvalidPanel : InvalidPanels) {
      if (!InvalidPanel)
        continue; // Null kontrolü

      // Panel için overlay bul
      TSharedPtr<SNexxNoteDrawingOverlay> Overlay;
      try {
        Overlay = GraphPanelToOverlayMap.FindRef(InvalidPanel);
      } catch (...) {
        continue;
      }

      // Overlay geçerliyse, kaldır
      if (Overlay.IsValid()) {
        try {
          // Parent overlay bul
          TSharedPtr<SOverlay> ParentOverlay;
          try {
            ParentOverlay =
                StaticCastSharedPtr<SOverlay>(InvalidPanel->GetParentWidget());
          } catch (...) {
            continue;
          }

          // Parent overlay slot'u kaldır
          if (ParentOverlay.IsValid()) {
            try {
              ParentOverlay->RemoveSlot(Overlay.ToSharedRef());
            } catch (...) {
              // Error handling silently
            }
          }

          // Overlay'i reset
          Overlay.Reset();
        } catch (...) {
          // Error removing overlay for panel
        }
      }

      // Haritalardan kaldır
      try {
        GraphPanelToOverlayMap.Remove(InvalidPanel);
        GraphPanelToSketchWidgetMap.Remove(InvalidPanel);
      } catch (...) {
        // Error removing from map
      }
    }
  }

  // 8. GÜVENLİK KATMANI - Hiç overlay güncellenemediyse, yeniden tarama yap
  if (!bAnyOverlayUpdated && bActive) {
    try {
      // Haritaları temizle
      GraphPanelToOverlayMap.Empty();
      GraphPanelToSketchWidgetMap.Empty();

      // Yeniden tara
      CheckForBlueprintEditors();

      // Yeni overlay'leri ayarla
      TMap<SGraphPanel *, TSharedPtr<SNexxNoteDrawingOverlay>> TempMap;
      TempMap.Append(GraphPanelToOverlayMap);

      for (auto &Pair : TempMap) {
        SGraphPanel *Panel = Pair.Key;
        TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;

        if (Panel && Overlay.IsValid()) {
          try {
            Overlay->SetSketchModeActive(bActive);
            Panel->Invalidate(EInvalidateWidgetReason::Layout);
            Panel->Invalidate(EInvalidateWidgetReason::Paint);
          } catch (...) {
            // Hata yoksay, bu ikinci denemededir
          }
        }
      }
    } catch (...) {
      // İkinci tarama sırasında genel hata
    }
  }

  // 9. GÜVENLİK KATMANI - Sketch modu kapanırken ekstra temizlik yap
  if (!bActive) {
    try {
      // Tüm panel'leri temizle
      TMap<SGraphPanel *, TSharedPtr<SNexxNoteDrawingOverlay>> CleanupMap;
      CleanupMap.Append(GraphPanelToOverlayMap);

      for (auto &Pair : CleanupMap) {
        SGraphPanel *Panel = Pair.Key;

        if (Panel) {
          try {
            // İmleç sıfırla
            Panel->SetCursor(EMouseCursor::Default);

            // Olayları temizle
            Panel->SetOnMouseButtonDown(FPointerEventHandler());
            Panel->SetOnMouseButtonUp(FPointerEventHandler());
            Panel->SetOnMouseMove(FPointerEventHandler());

            // Görünürlük ayarla
            Panel->SetVisibility(EVisibility::Visible);
            Panel->SetEnabled(true);

            // Mouse leave çağır
            Panel->OnMouseLeave(FPointerEvent());

            // Panel'i yenile
            Panel->Invalidate(EInvalidateWidgetReason::Layout);
            Panel->Invalidate(EInvalidateWidgetReason::Paint);
          } catch (...) {
            // Error during panel cleanup
          }
        }
      }

      // Olası tüm fare yakalamalarını bırak ve giriş durumunu sıfırla (ek
      // güvenlik)
      if (FSlateApplication::IsInitialized()) {
        FSlateApplication::Get().ReleaseAllPointerCapture();
        FSlateApplication::Get().ResetToDefaultInputSettings();
        FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
      }
    } catch (...) {
      // Error during extra cleanup
    }
  }

  // Son olarak callback'i çağır
  this->OnSketchModeStateChanged.ExecuteIfBound(bActive);
}

void FNexxNoteGraphPanelExtension::SetNoteModeActive(bool bActive) {
  // İlk önce durumu güncelle - herhangi bir hata durumunda bile bu değer
  // güncellenmiş olacak
  this->bNoteModeActive = bActive;

  // GÜVENLİK KATMANI 1: Slate uygulaması hazır mı kontrol et
  if (!FSlateApplication::IsInitialized()) {
    // Callback'i yine de çağır
    this->OnNoteModeStateChanged.ExecuteIfBound(bActive);
    return;
  }

  // Güvenli bir şekilde OnNoteModeStateChanged callback'ini çağır
  auto SafeCallOnNoteModeStateChanged = [this](bool bState) {
    try {
      this->OnNoteModeStateChanged.ExecuteIfBound(bState);
    } catch (...) {
      // Error handling silently
    }
  };

  // GÜVENLİK KATMANI 2: Aktifleştirme işlemi
  if (bActive) {
    try {
      // Eğer sketch modu aktifse, önce onu devre dışı bırak
      if (this->bSketchModeActive) {
        this->SetSketchModeActive(false);
      }

      // Blueprint editörleri bul ve haritayı güncelle
      try {
        CheckForBlueprintEditors();
      } catch (...) {
        // Blueprint editörleri kontrolü sırasında hata
      }

      // Overlay sayısını kontrol et
      if (this->GraphPanelToOverlayMap.Num() == 0) {
        // Not modu başarısız
        SafeCallOnNoteModeStateChanged(false);
        return;
      }

      // Geçersiz panel referanslarını topla
      TArray<SGraphPanel *> InvalidPanels;

      // Haritanın bir kopyasını al
      TMap<SGraphPanel *, TSharedPtr<SNexxNoteDrawingOverlay>> TempMap;
      TempMap.Append(this->GraphPanelToOverlayMap);

      // Tüm overlay'lerde not modunu ayarla
      for (auto &Pair : TempMap) {
        SGraphPanel *Panel = Pair.Key;
        TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;

        // Panel ve Overlay geçerli mi kontrol et
        if (!Panel || !Overlay.IsValid()) {
          if (Panel)
            InvalidPanels.Add(Panel);
          continue;
        }

        try {
          // Overlay'de not modunu aktif et
          Overlay->SetNoteModeActive(bActive);

          // İmleci normal olarak ayarla
          Panel->SetCursor(EMouseCursor::Default);
        } catch (...) {
          // Panel için not modu ayarlaması başarısız
          InvalidPanels.Add(Panel);
        }
      }

      // Geçersiz panelleri haritadan çıkar
      if (InvalidPanels.Num() > 0) {
        for (SGraphPanel *InvalidPanel : InvalidPanels) {
          if (!InvalidPanel)
            continue;

          // Haritalardan çıkar
          this->GraphPanelToOverlayMap.Remove(InvalidPanel);
          this->GraphPanelToSketchWidgetMap.Remove(InvalidPanel);
        }
      }

      // Not modunu aktif ettikten sonra, güvenli bir şekilde not oluşturmayı
      // dene
      try {
        CreateNoteAtCurrentPosition();
      } catch (...) {
        // Not oluşturma sırasında hata
      }

      // Not modu tek seferlik bir işlem, hemen devre dışı bırakıyoruz
      this->bNoteModeActive = false;

      // Tüm overlay'lerde not modunu devre dışı bırak
      for (auto &Pair : this->GraphPanelToOverlayMap) {
        TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;
        if (Overlay.IsValid()) {
          try {
            Overlay->SetNoteModeActive(false);
          } catch (...) {
            // Not modu devre dışı bırakılırken hata
          }
        }
      }
    } catch (...) {
      // Not modu aktivasyonunda genel hata
      this->bNoteModeActive = false;
    }
  }

  // Callback'i çağır
  SafeCallOnNoteModeStateChanged(this->bNoteModeActive);
}

void FNexxNoteGraphPanelExtension::CreateNoteAtCurrentPosition() {
  // Eğer etkin bir GraphPanel yoksa, çık
  if (this->GraphPanelToOverlayMap.Num() == 0) {
    return;
  }

  // Önce haritadan kopya oluştur
  TMap<SGraphPanel *, TSharedPtr<SNexxNoteDrawingOverlay>> TempPanelMap;
  TempPanelMap.Append(this->GraphPanelToOverlayMap);

  // Tüm graph panel'lerini gez
  for (auto &Pair : TempPanelMap) {
    // Her adımda null kontrolü
    SGraphPanel *GraphPanel = Pair.Key;

    // GraphPanel geçerli mi kontrol et
    if (!GraphPanel) {
      continue;
    }

    // Graph nesnesini kontrol et
    UEdGraph *Graph = nullptr;
    try {
      Graph = GraphPanel->GetGraphObj();
    } catch (...) {
      continue;
    }

    if (!Graph) {
      continue;
    }

    // Transaction kontrolü ekle
    FScopedTransaction *Transaction = nullptr;
    try {
      // Transaction başlat - bu işlemi geri alınabilir yap
      Transaction =
          new FScopedTransaction(LOCTEXT("NexxNoteCreateNote", "Create Note"));

      // Graph'ı modifiye edildiğini işaretle
      Graph->Modify();

      // GraphPanel'ın mevcut görünüm pozisyonunu al
      FVector2D ViewOffset;
      float ZoomAmount = 1.0f;

      try {
        ViewOffset = GraphPanel->GetViewOffset();
        ZoomAmount = GraphPanel->GetZoomAmount();
      } catch (...) {
        // Varsayılan değerleri kullan
        ViewOffset = FVector2D(0, 0);
        ZoomAmount = 1.0f;
      }

      // Panel boyutlarını al
      FVector2D PanelSize = FVector2D(1024, 768); // Varsayılan değer
      try {
        PanelSize = GraphPanel->GetTickSpaceGeometry().GetLocalSize();
      } catch (...) {
        // Varsayılan değeri kullanmaya devam et
      }

      // Görülebilir alanın Graph koordinatlarındaki boyutunu hesapla
      FVector2D VisibleSize = PanelSize / ZoomAmount;

      // Görülebilir alanın sağ üst köşesini hesapla
      // ViewOffset = Panelin sol üst köşesinin Graph koordinatlarındaki konumu
      FVector2D TopRightPosition;

      // Sağ üst köşe koordinatı: X = ViewOffset.X + VisibleSize.X, Y =
      // ViewOffset.Y Kenarlardan biraz uzakta olması için X'ten 300, Y'ye 100
      // ekleyelim
      TopRightPosition.X = ViewOffset.X + VisibleSize.X - 300.0f / ZoomAmount;
      TopRightPosition.Y = ViewOffset.Y + 100.0f / ZoomAmount;

      // Yeni bir not node oluştur
      UNexxNoteNode *NewNoteNode = nullptr;
      try {
        NewNoteNode = NewObject<UNexxNoteNode>(Graph);
      } catch (...) {
        // Not node'u oluşturulamadı
        if (Transaction) {
          delete Transaction;
          Transaction = nullptr;
        }
        continue;
      }

      if (!NewNoteNode) {
        // Not node'u oluşturulamadı
        if (Transaction) {
          delete Transaction;
          Transaction = nullptr;
        }
        continue;
      }

      // GraphPanel kontrolü - tekrar Graph'ın geçerliliğini doğrula
      if (!GraphPanel || !GraphPanel->GetGraphObj()) {
        if (Transaction) {
          delete Transaction;
          Transaction = nullptr;
        }
        continue;
      }

      // Düğümü graph'a eklemeden önce modifiye edildiğini işaretle
      NewNoteNode->Modify();

      // Graph'a node'u ekle
      try {
        Graph->AddNode(NewNoteNode, true);
      } catch (...) {
        // Not node'u graph'a eklenirken hata oluştu
        if (Transaction) {
          delete Transaction;
          Transaction = nullptr;
        }
        continue;
      }

      // Node pozisyonunu ayarla - hesaplanan sağ üst köşe pozisyonu
      NewNoteNode->NodePosX = TopRightPosition.X;
      NewNoteNode->NodePosY = TopRightPosition.Y;

      // Not içeriğini ayarla
      try {
        NewNoteNode->SetNoteText(FText::GetEmpty());

        // Not boyutunu ayarla (varsayılan)
        NewNoteNode->SetNoteSize(FVector2D(300.0f, 150.0f));
      } catch (...) {
        // Bu kritik değil, devam et
      }

      // Grafiği güncelle ve Blueprinti değiştirilmiş olarak işaretle
      UBlueprint *Blueprint = nullptr;
      try {
        Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
        if (Blueprint) {
          FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        }
      } catch (...) {
        // Bu kritik değil, devam et
      }

      // Node'u yenile ve graph paneli güncelle
      if (GraphPanel) {
        try {
          GraphPanel->RefreshNode(*NewNoteNode);
          GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
          GraphPanel->Invalidate(EInvalidateWidgetReason::Paint);

          // Node widget oluşturulduğundan emin ol
          TSharedPtr<SGraphNode> NodeWidget =
              GraphPanel->GetNodeWidgetFromGuid(NewNoteNode->NodeGuid);
          if (NodeWidget.IsValid()) {
            NodeWidget->UpdateGraphNode();
            NodeWidget->Invalidate(EInvalidateWidgetReason::Layout |
                                   EInvalidateWidgetReason::Paint);
          } else {
            // Tekrar deneyelim
            GraphPanel->RefreshNode(*NewNoteNode);
            GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
            GraphPanel->Invalidate(EInvalidateWidgetReason::Paint);
          }
        } catch (...) {
          // Panel güncelleme işlemi sırasında hata oluştu
        }
      } else {
      }

      // Transaction'ı temizle
      if (Transaction) {
        delete Transaction;
        Transaction = nullptr;
      }

      // Sadece ilk geçerli panelde not oluştur
      break;
    } catch (const std::exception &) {
      // Not oluşturma sırasında hata: %s
      if (Transaction) {
        delete Transaction;
        Transaction = nullptr;
      }
    } catch (...) {
      // Not oluşturma sırasında bilinmeyen bir hata oluştu
      if (Transaction) {
        delete Transaction;
        Transaction = nullptr;
      }
    }
  }
}

void FNexxNoteGraphPanelExtension::OnSketchModeDeactivated() {
  // Tüm fare yakalamalarını bırak
  if (FSlateApplication::IsInitialized()) {
    FSlateApplication::Get().ReleaseAllPointerCapture();

    FSlateApplication::Get().ResetToDefaultInputSettings();
    FSlateApplication::Get().ResetToDefaultPointerInputSettings();

    // Platform imlecini manuel olarak sıfırla
    TSharedPtr<ICursor> PlatformCursor =
        FSlateApplication::Get().GetPlatformCursor();
    if (PlatformCursor.IsValid()) {
      PlatformCursor->SetType(EMouseCursor::Default);
    }
  }

  this->bSketchModeActive = false;

  // Geçici kopyalar oluştur, çünkü işlem sırasında haritalar değişebilir
  TMap<SGraphPanel *, TSharedPtr<SNexxNoteDrawingOverlay>> TempOverlayMap =
      this->GraphPanelToOverlayMap;

  // Tüm çizim katmanlarını kaldır
  int32 ProcessedPanels = 0;
  for (auto &Pair : TempOverlayMap) {
    SGraphPanel *Panel = Pair.Key;
    TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;

    if (Panel && Overlay.IsValid()) {
      ProcessedPanels++;

      // Öncelikle panel üzerindeki tüm olay dinleyicilerini kaldır
      // Bu özellikle fare ve giriş olaylarını kaldırmak için önemli
      Panel->SetOnMouseButtonDown(FPointerEventHandler());
      Panel->SetOnMouseButtonUp(FPointerEventHandler());
      Panel->SetOnMouseMove(FPointerEventHandler());

      // İmleç sıfırlama
      Panel->SetCursor(EMouseCursor::Default);

      // ÖNEMLİ: Panel etkileşimi yeniden etkinleştir
      Panel->SetVisibility(EVisibility::Visible);
      Panel->SetEnabled(true);

      // ÖNEMLİ: Panel'in interaksiyon state'ini resetle
      Panel->OnMouseLeave(FPointerEvent());

      // Overlay'ı panelden kaldır - direkt parent overlay'i bulup ondan
      // kaldıralım
      TSharedPtr<SOverlay> ParentOverlay =
          StaticCastSharedPtr<SOverlay>(Panel->GetParentWidget());
      if (ParentOverlay.IsValid()) {
        ParentOverlay->RemoveSlot(Overlay.ToSharedRef());
      }

      // Panel'i yenile
      Panel->Invalidate(EInvalidateWidgetReason::Layout);
      Panel->Invalidate(EInvalidateWidgetReason::Paint);

      // Pencereyi tamamen yenile
      TSharedPtr<SWindow> ParentWindow =
          FSlateApplication::Get().FindWidgetWindow(Panel->AsShared());
      if (ParentWindow.IsValid()) {
        ParentWindow->Invalidate(EInvalidateWidgetReason::Layout);
        ParentWindow->Invalidate(EInvalidateWidgetReason::Paint);

        // Odağı pencereye ver
        FSlateApplication::Get().SetAllUserFocus(ParentWindow);

        // Değerlendirilecek widget'ı sıfırla
        ParentWindow->SetWidgetToFocusOnActivate(nullptr);
      }
    }
  }

  // İşlem tamamlandıktan sonra haritaları temizle
  this->GraphPanelToOverlayMap.Empty();
  this->GraphPanelToSketchWidgetMap.Empty();

  // ÖNEMLİ: State değişikliğini bildir - bu satır eksikti
  this->OnSketchModeStateChanged.ExecuteIfBound(false);
}

void FNexxNoteGraphPanelExtension::ClearAllOverlays() {
  // GÜVENLIK: Geçersiz durumda çökme olmasın diye ek güvenlik sağla
  if (!FSlateApplication::IsInitialized()) {
    UE_LOG(LogTemp, Warning,
           TEXT("[NexxNote] ClearAllOverlays: Slate not initialized"));
    // Güvenli şekilde haritaları temizle
    if (GraphPanelToOverlayMap.Num() > 0) {
      GraphPanelToOverlayMap.Empty();
    }
    if (GraphPanelToSketchWidgetMap.Num() > 0) {
      GraphPanelToSketchWidgetMap.Empty();
    }
    return;
  }

  // Overlay'leri temizle
  int32 SuccessfulRemovals = 0;

  // GÜVENLIK: Harita kopyası ile çalışalım ki orijinal etkilenmesin
  TMap<SGraphPanel *, TSharedPtr<SNexxNoteDrawingOverlay>> TempOverlayMap;
  if (GraphPanelToOverlayMap.Num() > 0) {
    TempOverlayMap = GraphPanelToOverlayMap;
  }

  // Panel başına temizlik
  for (auto &Pair : TempOverlayMap) {
    // GÜVENLIK: Panel ve overlay null olmasın
    SGraphPanel *Panel = Pair.Key;
    TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;

    if (!Panel || !Overlay.IsValid()) {
      continue;
    }

    try {
      // Panel'den Overlay'i kaldır
      try {
        // Önce Overlay'i devre dışı bırak - bu da çökmeleri önleyecek
        if (Overlay.IsValid()) {
          Overlay->SetSketchModeActive(false);
          Overlay->SetVisibility(EVisibility::Collapsed);
        }

        // Parent widget'ı bul
        TSharedPtr<SWidget> ParentWidget;

        // GÜVENLIK: Panel geçerli mi kontrol et
        if (Panel) {
          // GÜVENLIK: Panel güvenli mi kontrol et
          if (Panel->GetParentWidget().IsValid()) {
            ParentWidget = Panel->GetParentWidget();
          }
        }

        if (ParentWidget.IsValid()) {
          // GÜVENLIK: Cast etmeden önce doğru tip mi kontrol et
          if (ParentWidget->GetTypeAsString() == TEXT("SOverlay")) {
            TSharedPtr<SOverlay> ParentOverlay =
                StaticCastSharedPtr<SOverlay>(ParentWidget);
            if (ParentOverlay.IsValid() && Overlay.IsValid()) {
              try {
                ParentOverlay->RemoveSlot(Overlay.ToSharedRef());
                SuccessfulRemovals++;
              } catch (...) {
                // Panel için overlay kaldırılırken hata oluştu
                UE_LOG(LogTemp, Warning,
                       TEXT("[NexxNote] ClearAllOverlays: Error removing "
                            "overlay from parent"));
              }
            }
          }
        }
      } catch (...) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("[NexxNote] ClearAllOverlays: Error during overlay removal"));
      }

      // Panel eventlerini ve cursor'ı sıfırlama atlanıyor:
      // UE5.7'de SetCursor/SetEnabled çağrıları Invalidate() üzerinden
      // parent chain traverse yapar, panel destroy edilmişse crash olur.
      // Overlay kaldırma yeterli - panel kendi state'ini manage eder.
    } catch (...) {
      // Panel işleme sırasında genel hata yakalandı
      UE_LOG(LogTemp, Warning,
             TEXT("[NexxNote] ClearAllOverlays: General error during panel "
                  "processing"));
    }
  }

  // Güvenli bir şekilde haritaları temizle
  GraphPanelToOverlayMap.Empty();
  GraphPanelToSketchWidgetMap.Empty();

  // Slate yeniden kurulumu (sadece uygulama hala aktifse)
  if (FSlateApplication::IsInitialized()) {
    // try-catch ile koruma
    try {
      FSlateApplication::Get().ReleaseAllPointerCapture();
      FSlateApplication::Get().ResetToDefaultInputSettings();
      FSlateApplication::Get().ResetToDefaultPointerInputSettings();

      // Platform imlecini manuel olarak sıfırla
      TSharedPtr<ICursor> PlatformCursor =
          FSlateApplication::Get().GetPlatformCursor();
      if (PlatformCursor.IsValid()) {
        PlatformCursor->SetType(EMouseCursor::Default);
      }

      // Aktif pencereyi bul ve yenile - bunu daha güvenli yap
      try {
        TSharedPtr<SWindow> ActiveWindow =
            FSlateApplication::Get().GetActiveTopLevelWindow();
        if (ActiveWindow.IsValid()) {
          ActiveWindow->Invalidate(EInvalidateWidgetReason::Layout);
          ActiveWindow->Invalidate(EInvalidateWidgetReason::Paint);

          // Odağı pencereye ver
          FSlateApplication::Get().SetAllUserFocus(ActiveWindow);
          ActiveWindow->SetWidgetToFocusOnActivate(nullptr);
        }
      } catch (...) {
        // Error processing active window
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "[NexxNote] ClearAllOverlays: Error processing active window"));
      }
    } catch (...) {
      // Error during Slate cleanup
      UE_LOG(LogTemp, Warning,
             TEXT("[NexxNote] ClearAllOverlays: Error during Slate cleanup"));
    }
  }
}

// Tüm widget'ları bulan yardımcı fonksiyon artık NexxNoteHelpers namespace'inde
// tanımlı

// FNexxNoteInputProcessor sınıfının ResetCursor implementasyonu
void FNexxNoteInputProcessor::ResetCursor() {
  // Slate uygulamasında standart imlece geri dön
  TSharedPtr<ICursor> Cursor = FSlateApplication::Get().GetPlatformCursor();
  if (Cursor.IsValid()) {
    Cursor->SetType(EMouseCursor::Default);
  }

  // GraphPanel'in de imleç tipini sıfırla
  if (GraphPanel) {
    GraphPanel->SetCursor(EMouseCursor::Default);
  }

  // DrawingOverlay imleç tipini sıfırla
  if (DrawingOverlay.IsValid()) {
    DrawingOverlay->SetCursor(EMouseCursor::Default);
  }
}

void FNexxNoteGraphPanelExtension::SetDrawingColor(
    const FLinearColor &NewColor) {
  // Önce modül seviyesindeki değeri güncelle - en önemli kısım bu
  this->SavedDrawingColor = NewColor;

  // Tüm overlay'lerde rengi güncelle
  for (auto &Pair : this->GraphPanelToOverlayMap) {
    TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;
    if (Overlay.IsValid()) {
      Overlay->SetDrawingColor(NewColor);
    }
  }
}

void FNexxNoteGraphPanelExtension::SetDrawingThickness(float NewThickness) {
  // Önce modül seviyesindeki değeri güncelle - en önemli kısım bu
  this->SavedDrawingThickness = NewThickness;

  // Tüm overlay'lerde kalınlığı güncelle
  for (auto &Pair : this->GraphPanelToOverlayMap) {
    TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;
    if (Overlay.IsValid()) {
      Overlay->SetDrawingThickness(NewThickness);
    }
  }
}

FLinearColor FNexxNoteGraphPanelExtension::GetCurrentDrawingColor() const {
  // Herhangi bir aktif overlay varsa, onun rengini döndür
  for (auto &Pair : this->GraphPanelToOverlayMap) {
    TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;
    if (Overlay.IsValid()) {
      FLinearColor Color = Overlay->GetDrawingColor();
      return Color;
    }
  }

  // Overlay'den renk alamadıysak, kayıtlı değeri kullan
  return this->SavedDrawingColor;
}

float FNexxNoteGraphPanelExtension::GetCurrentDrawingThickness() const {
  // Herhangi bir aktif overlay varsa, onun kalınlığını döndür
  for (auto &Pair : this->GraphPanelToOverlayMap) {
    TSharedPtr<SNexxNoteDrawingOverlay> Overlay = Pair.Value;
    if (Overlay.IsValid()) {
      float Thickness = Overlay->GetDrawingThickness();
      return Thickness;
    }
  }

  // Overlay'den kalınlık alamadıysak, kayıtlı değeri kullan
  return this->SavedDrawingThickness;
}
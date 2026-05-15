// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "SGraphNodeNote.h"
#include "Editor/GraphEditor/Public/NodeFactory.h"
#include "GraphEditorSettings.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NexxNoteStyle.h"
#include "SGraphPanel.h"
#include "SlateOptMacros.h"
#include "UNexxNoteNode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// Fare tekerleği olaylarını yönlendirecek özel widget
class SScrollInterceptor : public SCompoundWidget {
public:
  SLATE_BEGIN_ARGS(SScrollInterceptor) : _Content() {}
  SLATE_DEFAULT_SLOT(FArguments, Content)
  SLATE_END_ARGS()

  void Construct(const FArguments &InArgs) {
    ChildSlot[InArgs._Content.Widget];
  }

  virtual FReply OnMouseWheel(const FGeometry &MyGeometry,
                              const FPointerEvent &MouseEvent) override {
    // Olayı ScrollBox'a yönlendir
    if (TargetScrollBox.IsValid()) {
      TargetScrollBox->OnMouseWheel(MyGeometry, MouseEvent);
    }

    // Olayı tüket, üst widget'lara geçmesini engelle
    return FReply::Handled();
  }

  void SetTargetScrollBox(TSharedPtr<SScrollBox> InScrollBox) {
    TargetScrollBox = InScrollBox;
  }

private:
  TSharedPtr<SScrollBox> TargetScrollBox;
};

#define LOCTEXT_NAMESPACE "NexxNoteGraphNode"

void SGraphNodeNote::Construct(const FArguments &InArgs,
                               UNexxNoteNode *InNode) {
  // Node referansını sakla
  NoteNodePtr = InNode;
  GraphNode = InNode;
  bIsHovering = false;

  // GÖRSELIN BOYUTU: Burada not görselinin varsayılan boyutunu
  // ayarlayabilirsiniz Not kağıdı PNG görüntüsünün genişlik ve yüksekliğini bu
  // değişkenlerle kontrol edin
  NoteImageWidth = 320.0f;  // PNG genişliği (piksel)
  NoteImageHeight = 430.0f; // PNG yüksekliği (piksel)

  // Placeholder durumu için değişken
  bIsPlaceholderActive = true;

  // Node boyutunu ayarla
  CurrentNodeSize.X = NoteImageWidth;
  CurrentNodeSize.Y = NoteImageHeight;
  InNode->SetNoteSize(CurrentNodeSize);

  // Not görselini yükle (artık sadece tek bir görsel)
  NoteFullBrush = FNexxNoteStyle::Get().GetBrush("NexxNote.NoteFull");

  // --- Görselin stretch olmadan çizilmesi için brush ayarları ---
  if (NoteFullBrush) {
    const_cast<FSlateBrush *>(NoteFullBrush)->Tiling =
        ESlateBrushTileType::NoTile;
    const_cast<FSlateBrush *>(NoteFullBrush)->DrawAs =
        ESlateBrushDrawType::Image;
  }

  // Komple widget'ı oluştur
  UpdateGraphNode();

  // ScrollInterceptor'a ScrollBox referansını ayarla
  if (ScrollInterceptor.IsValid() && ScrollBox.IsValid()) {
    ScrollInterceptor->SetTargetScrollBox(ScrollBox);
  }
}

void SGraphNodeNote::UpdateGraphNode() {
  // Mevcut içeriği temizle
  InputPins.Empty();
  OutputPins.Empty();

  // Reset variables
  RightNodeBox.Reset();
  LeftNodeBox.Reset();

  // Ana kutuyu oluştur
  TSharedPtr<SVerticalBox> MainVerticalBox = SNew(SVerticalBox);

  // Not bileşenlerini oluştur
  CreateNoteComponents();

  // Ana düğme içeriğini ekle
  this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

  this->GetOrAddSlot(ENodeZone::Center)
      .HAlign(HAlign_Center)
      .VAlign(VAlign_Center)[SNew(SBorder)
                                 .BorderImage(FAppStyle::GetBrush("NoBorder"))
                                 .Padding(0.0f)[MainVerticalBox.ToSharedRef()]];

  // Not içeriğini ekle
  MainVerticalBox->AddSlot()
      .HAlign(HAlign_Fill)
      .VAlign(VAlign_Fill)
      .AutoHeight()[NoteBox.ToSharedRef()];

  // Düğüm güncellemesini tamamla
  CreateBelowPinControls(MainVerticalBox);
}

void SGraphNodeNote::CreateNoteComponents() {
  NoteBox = SNew(SVerticalBox);

  // YAZININ FON BOYUTU (buradan değiştirebilirsin)
  const int32 FontSize = 12;

  // Text stilini siyah ve kalın yap
  FTextBlockStyle TextStyle =
      FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
  TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
  TextStyle.SetColorAndOpacity(FLinearColor::Black);

  // TEK PARÇA - Sabit boyut, overlay ile text üstte
  NoteBox->AddSlot()
      .HAlign(HAlign_Center)
      .VAlign(VAlign_Center)
      .AutoHeight()
          [SNew(SBox)
               // GÖRSELIN BOYUTU: Burada SBox'ın görüntüleme boyutunu
               // ayarlayabilirsiniz PNG görselinin görüntülenme boyutunu
               // ayarlamak için bu değerleri değiştirin
               .WidthOverride(NoteImageWidth)   // PNG genişliği
               .HeightOverride(NoteImageHeight) // PNG yüksekliği
               // Not dış alanları için el imleci (tutma simgesi)
               .Cursor(EMouseCursor::GrabHand)
                   [SNew(SOverlay) +
                    SOverlay::Slot()[SAssignNew(NoteImage, SImage)
                                         .Image(NoteFullBrush)
                                         .ColorAndOpacity(FLinearColor::White)]
                    // GENİŞ TIKANACAK ALAN - Tamamen saydam
                    +
                    SOverlay::Slot().Padding(
                        FMargin(40.0f, 70.0f, 10.0f,
                                10.0f)) // Not içindeki geniş tıklanabilir alan
                        [
                            // Geniş tıklama alanı için SBorder - tamamen saydam
                            SNew(SBorder)
                                .BorderImage(FAppStyle::GetBrush("NoBorder"))
                                .Padding(0)
                                .OnMouseButtonDown(
                                    this, &SGraphNodeNote::OnTextAreaClicked)
                                .Cursor(EMouseCursor::TextEditBeam)
                                .Visibility(
                                    EVisibility::
                                        HitTestInvisible) // Sadece tıklama
                                                          // algılama için,
                                                          // görünmez
                                                              [SNew(
                                                                  SBox) // Boş
                                                                        // bir
                                                                        // box -
                                                                        // sadece
                                                                        // tıklanabilir
                                                                        // alan
                                                                        // oluşturmak
                                                                        // için
  ]]
                    // METİN ALANI - Not içindeki yazı - doğrudan Overlay'a
                    // bağlı
                    +
                    SOverlay::Slot().Padding(FMargin(
                        55.0f, 130.0f, 30.0f,
                        30.0f)) // Sol, Üst, Sağ, Alt - Raptiyenin altından
                                // başlayıp kenarlardan içeride kalacak
                                    [SAssignNew(ScrollInterceptor,
                                                SScrollInterceptor)[
                                        // Metin için üst container - tıklamayı
                                        // algılamak için
                                        SNew(SBorder)
                                            .BorderImage(
                                                FAppStyle::GetBrush("NoBorder"))
                                            .Padding(0)
                                            .OnMouseButtonDown(
                                                this, &SGraphNodeNote::
                                                          OnTextAreaClicked)
                                            .Cursor(EMouseCursor::TextEditBeam)[
                                                // Kaydırılabilir alan için
                                                // SScrollBox ekle
                                                SAssignNew(ScrollBox,
                                                           SScrollBox)
                                                    .ScrollBarAlwaysVisible(
                                                        false) // Scroll bar
                                                               // sadece
                                                               // gerektiğinde
                                                               // görünsün
                                                    .ScrollBarThickness(
                                                        FVector2D(
                                                            8.0f,
                                                            8.0f)) // Scroll bar
                                                                   // kalınlığı
                                                +
                                                SScrollBox::Slot()[
                                                    // Sabit genişlikli bir SBox
                                                    // içine al, böylece metin
                                                    // kutusunun kendisi de
                                                    // sabit genişlikte olur
                                                    SNew(SBox).WidthOverride(600.0f) // Sabit genişlik - wrap değeriyle aynı
                                                        [SAssignNew(
                                                             TextContent,
                                                             SMultiLineEditableText)
                                                             .Text(
                                                                 NoteNodePtr
                                                                     ->NoteText)
                                                             .HintText(
                                                                 FText::FromString(
                                                                     TEXT("Clic"
                                                                          "k "
                                                                          "to "
                                                                          "add "
                                                                          "a "
                                                                          "not"
                                                                          "e")))
                                                             .AutoWrapText(true)
                                                             // Sabit genişlik
                                                             // değeri - zoom
                                                             // değişikliklerinden
                                                             // etkilenmez
                                                             .WrapTextAt(
                                                                 600.0f) // SBox
                                                                         // genişliğiyle
                                                                         // aynı
                                                             .OnTextChanged(
                                                                 this,
                                                                 &SGraphNodeNote::
                                                                     OnNoteTextChanged)
                                                             .OnTextCommitted(
                                                                 this,
                                                                 &SGraphNodeNote::
                                                                     OnNoteTextCommitted)
                                                             .WrappingPolicy(
                                                                 ETextWrappingPolicy::
                                                                     DefaultWrapping)
                                                             .AllowContextMenu(
                                                                 true)
                                                             .IsReadOnly(false)
                                                             // Text'in kendi
                                                             // içindeki kenar
                                                             // boşluğu
                                                             .Margin(FMargin(
                                                                 0, 0, 0, 0))
                                                             // Yazı stilini
                                                             // kullan (siyah ve
                                                             // kalın)
                                                             .TextStyle(
                                                                 &TextStyle)]]]]]]];
}

void SGraphNodeNote::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) {
  // Not için ek kontroller gerekmiyor
}

int32 SGraphNodeNote::OnPaint(const FPaintArgs &Args,
                              const FGeometry &AllottedGeometry,
                              const FSlateRect &MyCullingRect,
                              FSlateWindowElementList &OutDrawElements,
                              int32 LayerId, const FWidgetStyle &InWidgetStyle,
                              bool bParentEnabled) const {
  // ÖNEMLİ: Önce SGraphNode'un temel çizimini yapalım
  int32 BaseNodeLayerId = SGraphNode::OnPaint(
      Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
      InWidgetStyle, bParentEnabled);

  // Not nodeları için çizim yapmamıza gerek yok, çünkü NoteImage ve TextContent
  // Widget'ları zaten uygun şekilde oluşturulmuş ve hiyerarşiye eklenmiş
  // SGraphNode child elementleridir

  // Tek yapmamız gereken doğru layer ID'sini döndürmek
  // Çizim sırası çok önemli - aynı layer ID kullanıldığında sorun oluşuyor

  // Önemli: Normal blueprint nodelarının render akışını bozmamak için,
  // BaseNodeLayerId'den bir fazlasını döndürüyoruz
  // Bu sayede diğer nodelar bizim çizimimizden sonra doğru sırada çizilmeye
  // devam eder
  return BaseNodeLayerId + 1;
}

FVector2D SGraphNodeNote::ComputeDesiredSize(float) const {
  // Not boyutunu döndür
  return FVector2D(340.0f, 450.0f); // istediğin genişlik ve yükseklik
}

void SGraphNodeNote::MoveTo(const FVector2f &NewPosition, FNodeSet &NodeFilter,
                            bool bMarkDirty) {
  // Temel MoveTo işlemini çağır
  SGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);

  if (NoteNodePtr) {
    // Not node'unun pozisyonunu güncelle
    NoteNodePtr->Modify();

    // Blueprint'i değiştirilmiş olarak işaretle
    if (UBlueprint *Blueprint =
            FBlueprintEditorUtils::FindBlueprintForNodeChecked(NoteNodePtr)) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
  }
}

void SGraphNodeNote::EndUserInteraction() const {
  // Kullanıcı etkileşimi bittiğinde
  SGraphNode::EndUserInteraction();

  // Blueprint'i değiştirilmiş olarak işaretle
  if (NoteNodePtr) {
    if (UBlueprint *Blueprint =
            FBlueprintEditorUtils::FindBlueprintForNodeChecked(NoteNodePtr)) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
  }
}

FReply SGraphNodeNote::OnTextAreaClicked(const FGeometry &MyGeometry,
                                         const FPointerEvent &MouseEvent) {
  // Sol tıklama mı kontrol et
  if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
    // Doğrudan TextContent'a odaklan
    if (TextContent.IsValid()) {
      // Slate uygulaması başlatılmış mı kontrol et
      if (FSlateApplication::IsInitialized()) {
        // Doğrudan TextContent'a klavye odağını ver
        FSlateApplication::Get().SetKeyboardFocus(TextContent);

        // Eğer içerik boş değilse, imleci metnin sonunda konumlandır
        if (NoteNodePtr && !NoteNodePtr->NoteText.IsEmpty()) {
          // Metnin sonuna git
          TextContent->GoTo(ETextLocation::EndOfDocument);
        }
      }

      return FReply::Handled();
    }
  }

  // Tıklamayı ele alma, olayın diğer widget'lara da iletilmesini sağla
  return FReply::Unhandled();
}

void SGraphNodeNote::OnNoteTextChanged(const FText &NewText) {
  // Metin değiştiğinde sadece metni güncelle
  if (NoteNodePtr) {
    // Not metnini güncelle
    NoteNodePtr->SetNoteText(NewText);
  }
}

void SGraphNodeNote::OnNoteTextCommitted(const FText &NewText,
                                         ETextCommit::Type CommitType) {
  // Metin onaylandığında da not metnini güncelle
  if (NoteNodePtr) {
    // Not metnini güncelle
    NoteNodePtr->SetNoteText(NewText);

    // Blueprint'i değiştirilmiş olarak işaretle
    if (UBlueprint *Blueprint =
            FBlueprintEditorUtils::FindBlueprintForNodeChecked(NoteNodePtr)) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
  }
}

void SGraphNodeNote::ResizeNote(const FVector2D &NewSize) {
  // Not boyutunu güncelle
  if (NoteNodePtr) {
    // GÖRSELIN BOYUTU: Burada not boyutu dinamik olarak değiştiriliyor
    // Runtime sırasında not boyutunu değiştirmek istiyorsanız, bu fonksiyonu
    // çağırın Örnek: MyNoteNodeWidget->ResizeNote(FVector2D(400.0f, 300.0f));

    // Boyutu güncelle
    NoteNodePtr->SetNoteSize(NewSize);
    CurrentNodeSize = NewSize;

    // SBox boyutunu güncelle
    if (NoteBox.IsValid() && NoteBox->GetChildren()->Num() > 0) {
      TSharedRef<SWidget> BoxWidget = NoteBox->GetChildren()->GetChildAt(0);
      TSharedPtr<SBox> Box = StaticCastSharedRef<SBox>(BoxWidget);
      if (Box.IsValid()) {
        Box->SetWidthOverride(NewSize.X);
        Box->SetHeightOverride(NewSize.Y);
      }
    }

    // Blueprint'i değiştirilmiş olarak işaretle
    if (UBlueprint *Blueprint =
            FBlueprintEditorUtils::FindBlueprintForNodeChecked(NoteNodePtr)) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
  }
}

// Parent widget zincirini takip ederek SGraphPanel'i bul
TSharedPtr<SGraphPanel> SGraphNodeNote::FindParentGraphPanel() const {
  // GetParentWidget çağrısı kontrollü bir şekilde olmalı
  TSharedPtr<SWidget> ParentWidget = GetParentWidget();
  while (ParentWidget.IsValid()) {
    if (ParentWidget->GetTypeAsString() == TEXT("SGraphPanel")) {
      return StaticCastSharedPtr<SGraphPanel>(ParentWidget);
    }
    ParentWidget = ParentWidget->GetParentWidget();
  }

  return nullptr;
}

#undef LOCTEXT_NAMESPACE
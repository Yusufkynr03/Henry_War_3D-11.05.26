// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "SGraphNodeSketch.h"
#include "Containers/Array.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorSettings.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Input/Events.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layout/Geometry.h"
#include "Layout/SlateRect.h"
#include "Layout/WidgetPath.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "NodeFactory.h"
#include "Rendering/DrawElements.h"
#include "SGraphPanel.h"
#include "ScopedTransaction.h"
#include "Slate/SlateVectorArtData.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateStructs.h"
#include "UNexxSketchNode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

// Çizim kaydırma sorununu düzeltmek için SKETCH_PADDING'i 0 yapıyoruz
#define SKETCH_PADDING 0.0f

// Sabit değerlerin tanımları
const FVector2D SGraphNodeSketch::MinSize(75.0f, 75.0f);
const FVector2D SGraphNodeSketch::MaxSize(300.0f, 300.0f);

void SGraphNodeSketch::Construct(const FArguments &InArgs,
                                 UNexxSketchNode *InNode) {
  SketchNodePtr = InNode;
  this->SetCursor(EMouseCursor::Hand);
  bIsHoveringOverSketch = false;

  // Hover animasyonu ile ilgili değişkenler yerine,
  // doğrudan tam boyut kullanacağız
  CurrentNodeSize = FVector2D(150.0f, 150.0f);
  GraphNode = InNode;

  UpdateGraphNode();
}

void SGraphNodeSketch::UpdateGraphNode() {
  // Sketch node'ları için özel bir güncelleme yapıyoruz
  InputPins.Empty();
  OutputPins.Empty();

  // Node başlığını gizle
  this->GetNodeObj()->bCanRenameNode = false;

  // ÖNEMLİ: Burada boyut ayarı yap
  if (SketchNodePtr && SketchNodePtr->DrawPoints.Num() >= 2) {
    // Çizimin gerçek sınırlarını hesapla
    FVector2D MinPoint, MaxPoint;
    GetBoundingBoxForSketch(MinPoint, MaxPoint,
                            25.0f); // Biraz daha büyük padding

    // Çizim boyutunu hesapla
    FVector2D SketchSize = MaxPoint - MinPoint;
  }

  // Daha etkili fare algılaması için ÇOK ÖNEMLİ:
  // Node merkezine görünür bir Overlay Widget ekleyelim, bu seçilebilirliği
  // artıracak
  TSharedRef<SOverlay> MainOverlay = SNew(SOverlay);

  // İlk katman: Node'u temsil eden kısım - artık tam boyutta olacak
  MainOverlay->AddSlot()
      .HAlign(HAlign_Fill)
      .VAlign(VAlign_Fill)[SNew(SBox).Visibility(
          EVisibility::SelfHitTestInvisible) // Sadece görünür, tıklanamaz
  ];

  // İkinci katman: Fare etkileşimleri için görünür bir alan - tam boyutta
  MainOverlay->AddSlot()
      .HAlign(HAlign_Fill)
      .VAlign(VAlign_Fill)[SNew(SBox).Visibility(
          EVisibility::Visible) // Bu kutu artık tamamen etkileşimli!
  ];

  // Ana overlay'i node'un merkezine yerleştir
  this->GetOrAddSlot(ENodeZone::Center)
      .HAlign(HAlign_Center)
      .VAlign(VAlign_Center)[MainOverlay];

  // Gölgeleri ve genel görünümü devre dışı bırak
  this->GetNodeObj()->AdvancedPinDisplay = ENodeAdvancedPins::Hidden;

  // Node arka plan sınırlarını oluştur
  CreateBorderBackgroundForNode();
}

void SGraphNodeSketch::CreateBorderBackgroundForNode() {
  // Sketch node için özel arka plan oluştur
  // Node arka plan rengi ve çizimini ayarlamak için bu metodu kullanırız
  // Ama biz arka planı tamamen transparan yapmak istiyoruz

  // Not: SGraphNode'da doğrudan bu özelliklere erişemiyoruz
  // Bunun yerine OnPaint metodunda özel çizim yapacağız ve gölgeleri
  // kaldıracağız
}

// CreateRenderingResources - Render kaynakları için hazırlık
void SGraphNodeSketch::CreateRenderingResources() {
  // Render kaynakları için özel hazırlık
  // Bu fonksiyon node'un görsel bileşenlerini hazırlamak için kullanılır

  // Bu metod SGraphNode sınıfından override edilmiştir
  // Ve boş bir implementasyon yeterlidir, linker hatası alınmaması için
}

FReply SGraphNodeSketch::OnMouseMove(const FGeometry &MyGeometry,
                                     const FPointerEvent &MouseEvent) {
  // MUTLAKA temel sınıfın OnMouseMove metodunu çağır - bu kritik!
  // SGraphNode'un kendi dragging mekanizmasını çalıştırmasını sağlar
  return SGraphNode::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SGraphNodeSketch::OnMouseButtonDown(const FGeometry &MyGeometry,
                                           const FPointerEvent &MouseEvent) {
  const FVector2D CursorPos =
      MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

  // Çizimi tıklarsak ve farenin sol tuşuna basılıysa, sürüklemeye izin vermek
  // için fareyi yakala
  if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
      IsPointInsideSketch(CursorPos)) {
    // MUTLAKA temel sınıfın OnMouseButtonDown metodunu çağırmalıyız - bu
    // kritik! Bu, SGraphNode'un kendi dragging mekanizmasını çalıştırmasını
    // sağlar
    return SGraphNode::OnMouseButtonDown(MyGeometry, MouseEvent);
  }

  // Diğer tüm durumlar için temel sınıfın metodu
  return SGraphNode::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SGraphNodeSketch::OnMouseButtonUp(const FGeometry &MyGeometry,
                                         const FPointerEvent &MouseEvent) {
  // Fare yakalanmışsa
  if (HasMouseCapture()) {
    // Etkileşimi sonlandır ve fare yakalamayı bırak
    if (GraphNode) {
      GraphNode->Modify();
    }

    // Fare yakalamasını bırak
    return FReply::Handled().ReleaseMouseCapture();
  }

  // Temel sınıfın fare bırakma davranışını kullan
  return SGraphNode::OnMouseButtonUp(MyGeometry, MouseEvent);
}

int32 SGraphNodeSketch::OnPaint(const FPaintArgs &Args,
                                const FGeometry &AllottedGeometry,
                                const FSlateRect &MyCullingRect,
                                FSlateWindowElementList &OutDrawElements,
                                int32 LayerId,
                                const FWidgetStyle &InWidgetStyle,
                                bool bParentEnabled) const {
  // NOT: Orijinal SGraphNode çizimini çağırmıyoruz çünkü gölge ve diğer
  // elementleri çizmemesini istiyoruz Z-order sorununu çözmek için LayerId
  // değerini önemli ölçüde artırıyoruz böylece çizimlerimiz diğer graph
  // elemanlarının üzerinde görünecek
  int32 NewLayerId =
      LayerId + 1000; // Çok daha yüksek öncelikli bir z-order değeri

  // SketchNode geçerli mi kontrol et
  if (!SketchNodePtr || SketchNodePtr->DrawPoints.Num() < 2) {
    // Yeterli nokta yoksa çizim yapmaya gerek yok - varsayılan katmanı döndür
    return LayerId;
  }

  // GÜVENLIK KONTROLÜ: DrawPoints dizisinin geçerli olduğundan emin ol
  const TArray<FVector2D> &SafeDrawPoints = SketchNodePtr->DrawPoints;
  if (SafeDrawPoints.Num() < 2) {
    // Yeterli nokta yoksa varsayılan katmanı döndür
    return LayerId;
  }

  // Çizimin kapladığı alanı hesapla
  FVector2D MinPoint, MaxPoint;
  GetBoundingBoxForSketch(MinPoint, MaxPoint,
                          5.0f); // Bu padding sadece kutu göstergesi için

  // Çizim alanının boyutunu hesapla
  FVector2D Size = MaxPoint - MinPoint;

  // Arkaplan pozisyonunu hesapla
  FVector2D BackgroundPosition = MinPoint;

  // Node seçili mi?
  bool bIsSelected = false;

  // GetOwnerPanel yerine FindParentGraphPanel kullanıyoruz
  TSharedPtr<SGraphPanel> ParentPanel = FindParentGraphPanel();
  if (ParentPanel.IsValid()) {
    bIsSelected =
        ParentPanel->SelectionManager.SelectedNodes.Contains(GraphNode);
  }

  // Çizim alanının tamamını görünür hale getir - bu hitbox'u belirler
  FSlateLayoutTransform BackgroundTransform(BackgroundPosition);
  FPaintGeometry BackgroundGeometry =
      AllottedGeometry.ToPaintGeometry(Size, BackgroundTransform);

  // Arkaplan kutusunu çiz - HER ZAMAN BİR ARKAPLAN ÇİZ (bize ait proxy kutu)
  const FSlateBrush *NodeBrush = FAppStyle::GetBrush("WhiteBrush");

  // Tamamen transparan arkaplan ve çerçeve
  FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
  FLinearColor BorderColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
  float BorderThickness = 0.1f;

  // Arkaplan dolgusu çiz - kenar çizgisinden 1 katman altta - TAMAMEN SAYDAM
  FSlateDrawElement::MakeBox(OutDrawElements, NewLayerId++, BackgroundGeometry,
                             NodeBrush, ESlateDrawEffect::None,
                             BackgroundColor // Tamamen saydam
  );

  // Kenar çizgisi çiz - çerçeve şeklinde - TAMAMEN SAYDAM
  TArray<FVector2D> BorderPoints;
  BorderPoints.Add(FVector2D(0.0f, 0.0f));
  BorderPoints.Add(FVector2D(Size.X, 0.0f));
  BorderPoints.Add(FVector2D(Size.X, Size.Y));
  BorderPoints.Add(FVector2D(0.0f, Size.Y));
  BorderPoints.Add(FVector2D(0.0f, 0.0f));

  FSlateDrawElement::MakeLines(OutDrawElements, NewLayerId++,
                               BackgroundGeometry, BorderPoints,
                               ESlateDrawEffect::None,
                               BorderColor, // Tamamen saydam
                               false,
                               BorderThickness // Minimum değer
  );

  // Çizimi yap - node kutusundan bağımsız
  FLinearColor DrawColor = SketchNodePtr->DrawColor;
  DrawSketch(AllottedGeometry, OutDrawElements, NewLayerId++, InWidgetStyle,
             ShouldBeEnabled(bParentEnabled), DrawColor);

  return NewLayerId;
}

void SGraphNodeSketch::DrawSketch(const FGeometry &AllottedGeometry,
                                  FSlateWindowElementList &OutDrawElements,
                                  int32 LayerId,
                                  const FWidgetStyle &InWidgetStyle,
                                  bool bParentEnabled,
                                  const FLinearColor &Color) const {
  // GÜVENLIK: SketchNodePtr null olup olmadığını kontrol et
  if (!SketchNodePtr) {
    return;
  }

  // GÜVENLIK: DrawPoints dizisinin yeterli uzunlukta olduğunu kontrol et
  const TArray<FVector2D> &DrawPoints = SketchNodePtr->GetDrawingPoints();
  if (DrawPoints.Num() < 2) {
    return;
  }

  // GÜVENLIK: StrokeBreakIndices dizisinin geçerli olduğunu kontrol et
  const TArray<int32> &StrokeBreakIndices =
      SketchNodePtr->GetStrokeBreakIndices();

  // GÜVENLIK: DrawThickness ve DrawColor değerleri sıfır olmamalı
  float DrawThickness = FMath::Max(0.1f, SketchNodePtr->DrawThickness);
  FLinearColor DrawColor = SketchNodePtr->DrawColor;

  // Hover durumuna göre renk değiştir
  if (bIsHoveringOverSketch) {
    DrawColor = FLinearColor(DrawColor.R, DrawColor.G, DrawColor.B, 0.8f);
  }

  // Diziyi parçalar halinde çiz
  int32 StartIndex = 0;

  // GÜVENLIK: StrokeBreakIndices boşsa özel bir durum oluştur
  if (StrokeBreakIndices.Num() == 0) {
    // Tüm noktaları tek parça olarak çiz
    TArray<FVector2D> AllPoints;

    // GÜVENLIK: AllPoints için önceden boyut ayarla, böylece resize hatası
    // oluşmaz
    AllPoints.Reserve(DrawPoints.Num());

    for (const FVector2D &Point : DrawPoints) {
      AllPoints.Add(Point);
    }

    // Bu segment için en az 2 nokta var mı kontrol et
    if (AllPoints.Num() >= 2) {
      try {
        // Arka plan çizgisi
        FSlateDrawElement::MakeLines(
            OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(),
            AllPoints, ESlateDrawEffect::None,
            FLinearColor(DrawColor.R, DrawColor.G, DrawColor.B,
                         DrawColor.A * 0.5f), // Yarı saydam
            true,                             // Anti-aliasing
            DrawThickness + 0.5f              // Biraz daha kalın
        );

        // Ana çizgi
        FSlateDrawElement::MakeLines(
            OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(),
            AllPoints, ESlateDrawEffect::None, DrawColor,
            true, // Anti-aliasing
            DrawThickness);
      } catch (...) {
        // Çizim sırasında bir hata oluşursa sessizce atla
      }
    }

    return;
  }

  // Her segment için ayrı ayrı işlem yap
  for (int32 BreakIndex = 0; BreakIndex <= StrokeBreakIndices.Num();
       BreakIndex++) {
    int32 EndIndex = 0;

    // GÜVENLIK: BreakIndex sınırlarını kontrol et
    if (BreakIndex == StrokeBreakIndices.Num()) {
      // Son parça veya hiç kesme noktası yoksa
      EndIndex = DrawPoints.Num();
    } else if (BreakIndex >= 0 && BreakIndex < StrokeBreakIndices.Num()) {
      // GÜVENLIK: StrokeBreakIndices array içinde mi kontrol et
      // Belirli bir kesme noktasına kadar
      EndIndex = StrokeBreakIndices[BreakIndex];

      // GÜVENLIK: EndIndex sınırlarını kontrol et
      if (EndIndex <= 0 || EndIndex > DrawPoints.Num()) {
        EndIndex = DrawPoints.Num();
      }
    } else {
      // Geçersiz BreakIndex'i atla
      continue;
    }

    // GÜVENLIK: StartIndex ve EndIndex sınırlarını kontrol et
    if (StartIndex < 0) {
      StartIndex = 0;
    }

    if (EndIndex > DrawPoints.Num()) {
      EndIndex = DrawPoints.Num();
    }

    // Bu segment için en az 2 nokta var mı kontrol et
    if (EndIndex - StartIndex >= 2 && StartIndex < DrawPoints.Num()) {
      // Bu segment için noktaları ayrı bir diziye kopyala
      TArray<FVector2D> SegmentPoints;

      // GÜVENLIK: Bellek ayırmak için önceden boyutu ayarla, böylece resize
      // hatası oluşmaz
      SegmentPoints.Reserve(EndIndex - StartIndex);

      try {
        for (int32 i = StartIndex; i < EndIndex && i < DrawPoints.Num(); i++) {
          SegmentPoints.Add(DrawPoints[i]);
        }

        // GÜVENLIK: Segment en az 2 nokta içermiyorsa çizme
        if (SegmentPoints.Num() >= 2) {
          // Bu segmenti çiz - önce arka plan çizgisi
          FSlateDrawElement::MakeLines(
              OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(),
              SegmentPoints, ESlateDrawEffect::None,
              FLinearColor(DrawColor.R, DrawColor.G, DrawColor.B,
                           DrawColor.A * 0.5f), // Yarı saydam
              true,                             // Anti-aliasing
              DrawThickness + 0.5f              // Biraz daha kalın
          );

          // Sonra ana çizgiyi çiz
          FSlateDrawElement::MakeLines(
              OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(),
              SegmentPoints, ESlateDrawEffect::None, DrawColor,
              true, // Anti-aliasing
              DrawThickness);
        }
      } catch (...) {
        // Segment çizimi sırasında hata oluştu, sessizce atla
      }
    }

    // Sonraki segment için başlangıç indeksini güncelle
    StartIndex = EndIndex;
  }
}

// Ana Graph Panel'i bul - yardımcı fonksiyon
TSharedPtr<SGraphPanel> SGraphNodeSketch::FindParentGraphPanel() const {
  TSharedPtr<SWidget> Parent = this->GetParentWidget();
  while (Parent.IsValid()) {
    if (Parent->GetTypeAsString() == TEXT("SGraphPanel")) {
      return StaticCastSharedPtr<SGraphPanel>(Parent);
    }
    Parent = Parent->GetParentWidget();
  }
  return nullptr;
}

FVector2D SGraphNodeSketch::GetSketchSize() const {
  // Çizimin gerçek sınırlarını hesapla
  FVector2D MinPoint, MaxPoint;
  GetBoundingBoxForSketch(MinPoint, MaxPoint, 25.0f); // Padding ekleyelim

  // Çizim boyutunu hesapla ve döndür
  return MaxPoint - MinPoint;
}

void SGraphNodeSketch::MoveTo(const FVector2f &NewPosition,
                              FNodeSet &NodeFilter, bool bMarkDirty) {
  // Temel sınıfın MoveTo metodunu çağır - bu gerekli!
  SGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);

  // Çizim noktalarını taşıma - SketchNode geçerliyse
  if (SketchNodePtr && bMarkDirty) {
    // Değişiklikleri veri modelinde işaretle ve noktaları taşı
    GraphNode->Modify();
    SketchNodePtr->MoveSketchPoint(FVector2D(NewPosition.X, NewPosition.Y));
  }
}

void SGraphNodeSketch::EndUserInteraction() const {
  // Kullanıcı etkileşimi sona erdiğinde yapılacak işlemler
  if (SketchNodePtr) {
    SketchNodePtr->Modify();
  }
}

void SGraphNodeSketch::SetGraphNode(UEdGraphNode *InNode) {
  GraphNode = InNode;

  // UNexxSketchNode referansını güncelleyelim
  SketchNodePtr = Cast<UNexxSketchNode>(GraphNode);
}

// ComputeDesiredSize metodunu değiştir - artık her zaman çizim kadar büyük
// olacak
FVector2D SGraphNodeSketch::ComputeDesiredSize(float) const {
  // Çizimin gerçek sınırlarını hesapla
  FVector2D MinPoint, MaxPoint;
  GetBoundingBoxForSketch(MinPoint, MaxPoint, 25.0f); // Geniş padding

  // Gerçek çizim boyutunu hesapla
  FVector2D ActualSketchSize = MaxPoint - MinPoint;

  // Artık boyut değiştiren bir hover sistemi yok, direkt olarak çizim boyutunu
  // döndürüyoruz
  FVector2D ComputedSize = ActualSketchSize;

  // Minimum bir boyut olmalı
  ComputedSize.X = FMath::Max(ComputedSize.X, 50.0f);
  ComputedSize.Y = FMath::Max(ComputedSize.Y, 50.0f);

  return ComputedSize;
}

// Çizimin sınır kutusunu hesaplayan yardımcı fonksiyon
void SGraphNodeSketch::GetBoundingBoxForSketch(FVector2D &OutMinPoint,
                                               FVector2D &OutMaxPoint,
                                               float Padding) const {
  // GÜVENLIK: SketchNodePtr null ise veya çizim noktaları yetersizse
  if (!SketchNodePtr || !SketchNodePtr->DrawPoints.Num()) {
    // Yetersiz nokta durumunda varsayılan küçük bir kutu
    OutMinPoint = FVector2D(-25.0f, -25.0f);
    OutMaxPoint = FVector2D(25.0f, 25.0f);
    return;
  }

  // GÜVENLIK: Nokta sayısı en az 1 olmalı
  const TArray<FVector2D> &SafeDrawPoints = SketchNodePtr->DrawPoints;
  if (SafeDrawPoints.Num() < 1) {
    // Yetersiz nokta durumunda varsayılan küçük bir kutu
    OutMinPoint = FVector2D(-25.0f, -25.0f);
    OutMaxPoint = FVector2D(25.0f, 25.0f);
    return;
  }

  try {
    // İlk noktayı min/max olarak başlat
    OutMinPoint = SafeDrawPoints[0];
    OutMaxPoint = SafeDrawPoints[0];

    // Tüm çizim noktalarını kontrol et
    for (const FVector2D &Point : SafeDrawPoints) {
      // Minimum X ve Y
      OutMinPoint.X = FMath::Min(OutMinPoint.X, Point.X);
      OutMinPoint.Y = FMath::Min(OutMinPoint.Y, Point.Y);

      // Maximum X ve Y
      OutMaxPoint.X = FMath::Max(OutMaxPoint.X, Point.X);
      OutMaxPoint.Y = FMath::Max(OutMaxPoint.Y, Point.Y);
    }
  } catch (...) {
    // Hata durumunda varsayılan kutu döndür
    OutMinPoint = FVector2D(-25.0f, -25.0f);
    OutMaxPoint = FVector2D(25.0f, 25.0f);
    return;
  }

  // Padding ekle
  OutMinPoint.X -= Padding;
  OutMinPoint.Y -= Padding;
  OutMaxPoint.X += Padding;
  OutMaxPoint.Y += Padding;

  // GÜVENLIK: Geçersiz değerleri kontrol et ve sınırlandır
  if (!FMath::IsFinite(OutMinPoint.X) || FMath::IsNaN(OutMinPoint.X))
    OutMinPoint.X = -25.0f;
  if (!FMath::IsFinite(OutMinPoint.Y) || FMath::IsNaN(OutMinPoint.Y))
    OutMinPoint.Y = -25.0f;
  if (!FMath::IsFinite(OutMaxPoint.X) || FMath::IsNaN(OutMaxPoint.X))
    OutMaxPoint.X = 25.0f;
  if (!FMath::IsFinite(OutMaxPoint.Y) || FMath::IsNaN(OutMaxPoint.Y))
    OutMaxPoint.Y = 25.0f;

  // Minimum boyut sınırı
  if (OutMaxPoint.X - OutMinPoint.X < 10.0f) {
    float MidX = (OutMinPoint.X + OutMaxPoint.X) * 0.5f;
    OutMinPoint.X = MidX - 5.0f;
    OutMaxPoint.X = MidX + 5.0f;
  }

  if (OutMaxPoint.Y - OutMinPoint.Y < 10.0f) {
    float MidY = (OutMinPoint.Y + OutMaxPoint.Y) * 0.5f;
    OutMinPoint.Y = MidY - 5.0f;
    OutMaxPoint.Y = MidY + 5.0f;
  }
}

// Hit test fonksiyonu - tüm kutu için çalışacak şekilde güncellendi
bool SGraphNodeSketch::HitTest(const FGeometry &MyGeometry,
                               FVector2D InAbsoluteCursorPosition) const {
  // Fare pozisyonunu yerel koordinatlara dönüştür
  FVector2D LocalMouseCoords =
      MyGeometry.AbsoluteToLocal(InAbsoluteCursorPosition);

  // Graf panelinin zoom değerini al
  TSharedPtr<SGraphPanel> ParentGraphPanel = FindParentGraphPanel();
  float ZoomFactor = 1.0f;

  if (ParentGraphPanel.IsValid()) {
    ZoomFactor = ParentGraphPanel->GetZoomAmount();
  }

  // Zoom faktörünü dikkate alarak BoundingBox'ı hesapla
  FVector2D MinPoint, MaxPoint;
  GetBoundingBoxForSketch(MinPoint, MaxPoint, 25.0f * ZoomFactor);

  // Kutunun sınırları içinde mi kontrol et
  bool bInBoundingBox =
      (LocalMouseCoords.X >= MinPoint.X && LocalMouseCoords.X <= MaxPoint.X &&
       LocalMouseCoords.Y >= MinPoint.Y && LocalMouseCoords.Y <= MaxPoint.Y);

  // Kutu içinde ise, direkt olarak hit testi başarılı
  if (bInBoundingBox) {
    return true;
  }

  // Kutu dışındaysa, çizgiye yakınlık ikincil bir test olarak yapılsın
  if (SketchNodePtr && SketchNodePtr->DrawPoints.Num() >= 2) {
    // En yakın çizgi parçasına olan mesafeyi hesapla
    float ClosestDistanceSquared = FLT_MAX;

    for (int32 i = 0; i < SketchNodePtr->DrawPoints.Num() - 1; i++) {
      // FVector2D'yi FVector'e dönüştür (Z=0 olarak)
      FVector PointA3D(SketchNodePtr->DrawPoints[i].X,
                       SketchNodePtr->DrawPoints[i].Y, 0.0f);
      FVector PointB3D(SketchNodePtr->DrawPoints[i + 1].X,
                       SketchNodePtr->DrawPoints[i + 1].Y, 0.0f);
      FVector TestPoint3D(LocalMouseCoords.X, LocalMouseCoords.Y, 0.0f);

      // Noktadan çizgi parçasına uzaklık hesapla
      float DistanceSquared =
          FMath::PointDistToSegmentSquared(TestPoint3D, PointA3D, PointB3D);

      // En yakın mesafeyi güncelle
      ClosestDistanceSquared =
          FMath::Min(ClosestDistanceSquared, DistanceSquared);
    }

    // Çizgiye çok yakınsa (kutu dışında olsa bile), hit testi başarılı sayılsın
    // Zoom seviyesine göre eşik değerini ayarla
    const float HitThresholdSquared = 150.0f * ZoomFactor * ZoomFactor;
    if (ClosestDistanceSquared <= HitThresholdSquared) {
      return true;
    }
  }

  // Kutu içinde değil ve çizgiye de yakın değilse, hit testi başarısız
  return false;
}

// Tick metodunu düzelt - artık hover animasyonu yok
void SGraphNodeSketch::Tick(const FGeometry &AllottedGeometry,
                            const double InCurrentTime,
                            const float InDeltaTime) {
  // Temel sınıfın Tick metodunu çağır
  SGraphNode::Tick(AllottedGeometry, InCurrentTime,
                   static_cast<float>(InDeltaTime));

  // Artık hover sistemi yok, bu yüzden tick içinde boyut değişimi yapmıyoruz
  // Çizim her zaman gerçek boyutunda gösterilecek
}

// Fare widget alanına girdiğinde
void SGraphNodeSketch::OnMouseEnter(const FGeometry &MyGeometry,
                                    const FPointerEvent &MouseEvent) {
  // Artık hover'da boyut değişimi yok, sadece el imlecini göster
  this->SetCursor(EMouseCursor::GrabHand);

  // Temel sınıfın metodunu çağır
  SGraphNode::OnMouseEnter(MyGeometry, MouseEvent);
}

// Fare widget alanından çıktığında
void SGraphNodeSketch::OnMouseLeave(const FPointerEvent &MouseEvent) {
  // İmleç stilini normale döndür
  this->SetCursor(EMouseCursor::Default);

  // Temel sınıfın metodunu çağır
  SGraphNode::OnMouseLeave(MouseEvent);
}

// IsPointInsideSketch fonksiyonu - sadece hit test için kullanıyoruz artık
bool SGraphNodeSketch::IsPointInsideSketch(const FVector2D &Point) const {
  // Graf panelinin zoom değerini al
  TSharedPtr<SGraphPanel> ParentGraphPanel = FindParentGraphPanel();
  float ZoomFactor = 1.0f;

  if (ParentGraphPanel.IsValid()) {
    ZoomFactor = ParentGraphPanel->GetZoomAmount();
  }

  // Çizim sınırlarını bul - zoom faktörünü dikkate al
  FVector2D MinPoint, MaxPoint;
  GetBoundingBoxForSketch(MinPoint, MaxPoint, 25.0f * ZoomFactor);

  // Sınır kutusu içinde olup olmadığını kontrol et
  bool bInsideBoundingBox = (Point.X >= MinPoint.X && Point.X <= MaxPoint.X &&
                             Point.Y >= MinPoint.Y && Point.Y <= MaxPoint.Y);

  if (bInsideBoundingBox) {
    return true;
  }

  // Çizgiye yakınlık testi
  if (SketchNodePtr && SketchNodePtr->DrawPoints.Num() >= 2) {
    float ClosestDistanceSquared = FLT_MAX;
    for (int32 i = 0; i < SketchNodePtr->DrawPoints.Num() - 1; i++) {
      FVector PointA3D(SketchNodePtr->DrawPoints[i].X,
                       SketchNodePtr->DrawPoints[i].Y, 0.0f);
      FVector PointB3D(SketchNodePtr->DrawPoints[i + 1].X,
                       SketchNodePtr->DrawPoints[i + 1].Y, 0.0f);
      FVector TestPoint3D(Point.X, Point.Y, 0.0f);

      float DistanceSquared =
          FMath::PointDistToSegmentSquared(TestPoint3D, PointA3D, PointB3D);
      ClosestDistanceSquared =
          FMath::Min(ClosestDistanceSquared, DistanceSquared);
    }

    const float ThresholdSquared = 150.0f * ZoomFactor * ZoomFactor;
    if (ClosestDistanceSquared <= ThresholdSquared) {
      return true;
    }
  }

  return false;
}

// GetBoundingBoxForSketch yardımcı fonksiyonu
FBox2D SGraphNodeSketch::GetBoundingBoxForSketch() const {
  FVector2D MinPoint(0.0f, 0.0f), MaxPoint(0.0f, 0.0f);

  // SketchNode geçerli mi kontrol et
  if (SketchNodePtr && SketchNodePtr->DrawPoints.Num() >= 2) {
    // İlk noktayı min/max olarak başlat
    MinPoint = SketchNodePtr->DrawPoints[0];
    MaxPoint = SketchNodePtr->DrawPoints[0];

    // Tüm çizim noktalarını kontrol et
    for (const FVector2D &Point : SketchNodePtr->DrawPoints) {
      // Minimum X ve Y
      MinPoint.X = FMath::Min(MinPoint.X, Point.X);
      MinPoint.Y = FMath::Min(MinPoint.Y, Point.Y);

      // Maximum X ve Y
      MaxPoint.X = FMath::Max(MaxPoint.X, Point.X);
      MaxPoint.Y = FMath::Max(MaxPoint.Y, Point.Y);
    }

    // Çerçeve çizmek için sınırları biraz genişlet
    MinPoint -= FVector2D(10.0f, 10.0f);
    MaxPoint += FVector2D(10.0f, 10.0f);

    // Sınır kutusunu oluştur
    return FBox2D(MinPoint, MaxPoint);
  } else {
    // Geçerli bir çizim yoksa, varsayılan küçük bir kutu
    MinPoint = FVector2D(-25.0f, -25.0f);
    MaxPoint = FVector2D(25.0f, 25.0f);

    // Varsayılan kutuyu döndür
    return FBox2D(MinPoint, MaxPoint);
  }
}

// ForceRefreshNode
void SGraphNodeSketch::ForceRefreshNode() {
  // Kendi widget'ımızı yenile
  Invalidate(EInvalidateWidgetReason::Layout);
  Invalidate(EInvalidateWidgetReason::Paint);

  // Graf panelini bul ve güncelle
  TSharedPtr<SGraphPanel> Panel = FindParentGraphPanel();
  if (!Panel.IsValid()) {
    return;
  }

  // Panel yenileme
  Panel->Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SGraphNodeSketch::GetPosition() const {
  return SGraphNode::GetPosition();
}

const FSlateBrush *SGraphNodeSketch::GetShadowBrush(bool bSelected) const {
  return FStyleDefaults::GetNoBrush();
}
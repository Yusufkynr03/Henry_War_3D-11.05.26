// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "SNexxNoteDrawingOverlay.h"
#include "NexxNoteHelpers.h"
#include "EdGraphNode_Comment.h"
#include "BlueprintEditorModule.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "UNexxSketchNode.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"

// FindAllWidgetsRecursive fonksiyonu artık NexxNoteHelpers namespace'inde tanımlı

#define LOCTEXT_NAMESPACE "NexxNoteDrawingOverlay"

SNexxNoteDrawingOverlay::SNexxNoteDrawingOverlay()
    : GraphPanel(nullptr)
    , bSketchModeActive(false)
    , bNoteModeActive(false)
    , bDrawing(false)
    , bIsMouseInsidePanel(false)
    , bIsMouseLeftButtonDown(false)
    , bWasShiftKeyDown(false)
    , DrawColor(FLinearColor::Yellow)
    , DrawThickness(2.0f)
    , LastMousePos(FVector2D::ZeroVector)
{
    // Tick etkinleştir - fare konumunu sürekli olarak kontrol edebilmek için
    bCanTick = true;
}

void SNexxNoteDrawingOverlay::Construct(const FArguments& InArgs, SGraphPanel* InGraphPanel)
{
    GraphPanel = InGraphPanel;

    // Bilgilendirme textleri için TextBlock'ları oluştur
    // Shift durumu texti (koyu sarı renk)
    ShiftStateTextBlock = SNew(STextBlock)
        .Visibility(EVisibility::Collapsed)
        .ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.0f, 1.0f)) // Koyu sarı
        .ShadowOffset(FVector2D(1.0f, 1.0f))
        .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)) // Kalın font
        .Text(FText::FromString(TEXT("[Shift ACTIVE]")));
    
    // Durum bilgisi texti (beyaz)
    StatusTextBlock = SNew(STextBlock)
        .Visibility(EVisibility::Collapsed)
        .ColorAndOpacity(FLinearColor::White)
        .ShadowOffset(FVector2D(1.0f, 1.0f))
        .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)); // Kalın font

    ChildSlot
    [
        SNew(SOverlay)
        +SOverlay::Slot()
        [
            // Boş widget
        SNullWidget::NullWidget
        ]
        +SOverlay::Slot()
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(FMargin(0, 10, 248, 0)) // Zoom yazısının soluna yerleştirilecek Shift durumu
        [
            ShiftStateTextBlock.ToSharedRef()
        ]
        +SOverlay::Slot()
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(FMargin(0, 10, 110, 0)) // Zoom yazısının yanına yerleştirilecek durum bilgisi
        [
            StatusTextBlock.ToSharedRef()
        ]
    ];
    
    // Tick etkinleştir - fare konumunu sürekli olarak kontrol edebilmek için
    bCanTick = true;
    
    // Aktif olduğunda fare imlecini değiştir
    SetCursor(EMouseCursor::Crosshairs);
}

void SNexxNoteDrawingOverlay::OptimizeDrawPoints(const TArray<FVector2D>& InPoints, TArray<FVector2D>& OutPoints, float MinDistance) const
{
    // Hiç nokta yoksa boş dön
    if (InPoints.Num() == 0)
    {
        OutPoints.Empty();
        return;
    }
    
    // İlk noktayı her zaman ekle
    OutPoints.Add(InPoints[0]);
    
    // Her noktayı kontrol et
    for (int32 i = 1; i < InPoints.Num(); i++)
    {
        // Son eklenen noktadan uzaklık hesapla
        float Distance = FVector2D::Distance(OutPoints.Last(), InPoints[i]);
        
        // Mesafe minimum değerden büyükse bu noktayı ekle
        if (Distance > MinDistance)
        {
            OutPoints.Add(InPoints[i]);
        }
    }
    
    // Son noktayı her zaman ekle (eğer zaten eklenmediyse)
    if (OutPoints.Last() != InPoints.Last())
    {
        OutPoints.Add(InPoints.Last());
    }
}

// Catmull-Rom spline değeri hesapla (Smooth çizim için)
FVector2D SNexxNoteDrawingOverlay::CatmullRomSpline(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, float T) const
{
    const float T2 = T * T;
    const float T3 = T2 * T;
    
    const float B1 = 0.5f * (-T3 + 2.0f * T2 - T);
    const float B2 = 0.5f * (3.0f * T3 - 5.0f * T2 + 2.0f);
    const float B3 = 0.5f * (-3.0f * T3 + 4.0f * T2 + T);
    const float B4 = 0.5f * (T3 - T2);
    
    return (P0 * B1 + P1 * B2 + P2 * B3 + P3 * B4);
}

// Noktalar arasına smooth çizgiler ekle
void SNexxNoteDrawingOverlay::SmoothDrawPoints(const TArray<FVector2D>& InPoints, TArray<FVector2D>& OutPoints, int32 SubdivisionCount) const
{
    if (InPoints.Num() < 3)
    {
        // Yeterli nokta yoksa, orijinal noktaları kullan
        OutPoints = InPoints;
        return;
    }
    
    // Daha fazla alt bölüm için hafızada yer aç
    OutPoints.Empty(InPoints.Num() * SubdivisionCount);
    
    // İlk noktayı ekle
    OutPoints.Add(InPoints[0]);
    
    // Her nokta arasını smooth eğrilerle doldur
    for (int32 i = 0; i < InPoints.Num() - 1; i++)
    {
        // Catmull-Rom spline için 4 kontrol noktası seç
        FVector2D P0, P1, P2, P3;
        
        // İlk nokta için özel durum
        if (i == 0)
        {
            // İlk nokta için, P0 noktasını ekstra ekstrapolasyon noktası oluştur
            P1 = InPoints[i];
            P0 = P1 - (InPoints[i+1] - P1);
        }
        else
        {
            P0 = InPoints[i-1];
            P1 = InPoints[i];
        }
        
        P2 = InPoints[i+1];
        
        // Son nokta için özel durum
        if (i+2 >= InPoints.Num())
        {
            // Son nokta için, P3 noktasını ekstra ekstrapolasyon oluştur
            P3 = P2 + (P2 - P1);
        }
        else
        {
            P3 = InPoints[i+2];
        }
        
        // İlk noktayı zaten ekledik, şimdi ara noktaları ekleyelim
        if (i != 0)
        {
            OutPoints.Add(P1);
        }
        
        // Ara noktaları spline ile hesapla - daha fazla alt bölüm için daha pürüzsüz çizgi
        for (int32 j = 1; j < SubdivisionCount; j++)
        {
            float T = (float)j / (float)SubdivisionCount;
            FVector2D SubPoint = CatmullRomSpline(P0, P1, P2, P3, T);
            OutPoints.Add(SubPoint);
        }
    }
    
    // Son noktayı ekle
    OutPoints.Add(InPoints.Last());
}

int32 SNexxNoteDrawingOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // Temel sınıf çizimini yap
    int32 DrawingLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    
    // Çizilecek nokta yoksa erken dön
    if (DrawPoints.Num() < 2)
    {
        return DrawingLayerId;
    }
    
    // GraphPanel zorunlu olmalı
    if (!GraphPanel)
    {
        return DrawingLayerId;
    }
    
    // GraphPanel'ın zoom ve pan değerlerini al
    float ZoomAmount = GraphPanel->GetZoomAmount();
    FVector2D ViewOffset = GraphPanel->GetViewOffset();
    
    // Çizim parçalarını işle
    int32 StartIndex = 0;
    
    // Önce çizim parçalarını segmentlere ayır
    TArray<TArray<FVector2D>> SegmentPoints;
    
    // Her çizim parçasını ayrı ayrı işle
    for (int32 BreakIndex = 0; BreakIndex <= StrokeBreakIndices.Num(); BreakIndex++)
    {
        int32 EndIndex;
        
        // Son parça için veya hiç parça yoksa
        if (BreakIndex == StrokeBreakIndices.Num())
        {
            EndIndex = DrawPoints.Num();
        }
        else
        {
            EndIndex = StrokeBreakIndices[BreakIndex];
        }
        
        // En az iki nokta varsa bu parçayı ayrı bir segment olarak ekle
        if (EndIndex - StartIndex >= 2)
        {
            // Bu parça için noktaları ayır
            TArray<FVector2D> CurrentSegment;
            for (int32 i = StartIndex; i < EndIndex; i++)
            {
                CurrentSegment.Add(DrawPoints[i]);
            }
            
            // Segment listesine ekle
            SegmentPoints.Add(CurrentSegment);
        }
        
        // Sonraki parça için başlangıç indeksi
        StartIndex = EndIndex;
    }
    
    // Her segment için ayrı ayrı işlem yap
    for (int32 SegmentIndex = 0; SegmentIndex < SegmentPoints.Num(); SegmentIndex++)
    {
        // Her segment için optimize edilmiş noktaları hesapla
        TArray<FVector2D> OptimizedSegment;
        OptimizeDrawPoints(SegmentPoints[SegmentIndex], OptimizedSegment);
        
        // Yetersiz nokta kalmışsa bu segmenti atla
        if (OptimizedSegment.Num() < 2)
        {
            continue;
        }
        
        // Her segment için yumuşatılmış noktaları hesapla
        TArray<FVector2D> SmoothedSegment;
        SmoothDrawPoints(OptimizedSegment, SmoothedSegment, 3);
        
        // Yumuşatılmış noktaları panel koordinatlarına dönüştür
        TArray<FVector2D> PanelSegmentPoints;
        for (const FVector2D& GraphPoint : SmoothedSegment)
        {
            // Graph -> Panel dönüşümü: (Graph - ViewOffset) * ZoomAmount
            FVector2D PanelPoint = (GraphPoint - ViewOffset) * ZoomAmount;
            PanelSegmentPoints.Add(PanelPoint);
        }
        
        // Önce arka plan çizgisi (daha kalın ve yarı şeffaf)
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            DrawingLayerId + 2,
            AllottedGeometry.ToPaintGeometry(),
            PanelSegmentPoints,
            ESlateDrawEffect::None,
            FLinearColor(DrawColor.R, DrawColor.G, DrawColor.B, DrawColor.A * 0.5f), // Yarı saydam
            true, // Anti-aliasing
            DrawThickness + 0.5f // Biraz daha kalın
        );
        
        // Sonra ana çizgi
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            DrawingLayerId + 3,
            AllottedGeometry.ToPaintGeometry(),
            PanelSegmentPoints,
            ESlateDrawEffect::None,
            DrawColor, // Tam opak
            true, // Anti-aliasing
            DrawThickness
        );
    }
    
    return DrawingLayerId + 3;
}

// Panel içerisinde olup olmadığını kontrol et
bool SNexxNoteDrawingOverlay::IsPositionInsidePanel(const FVector2D& ScreenPosition) const
{
    if (!GraphPanel)
    {
        return false;
    }
    
    FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
    return PanelGeometry.IsUnderLocation(ScreenPosition);
}

// MyGeometry parametresi ile pozisyon tespiti (daha hassas)
bool SNexxNoteDrawingOverlay::IsPositionInsidePanel(const FGeometry& MyGeometry, const FVector2D& ScreenPosition) const
{
    if (!GraphPanel)
    {
        return false;
    }
    
    FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
    return PanelGeometry.IsUnderLocation(ScreenPosition);
}

// HitTest fonksiyonu - fare tıklamaları ve etkileşimler için önemli
bool SNexxNoteDrawingOverlay::HitTest(const FGeometry& MyGeometry, FVector2D InAbsoluteCursorPosition) 
{
    // Sadece Sketch modu aktifse HitTest yap, Not modu için yapma
    if (!bSketchModeActive)
    {
        return false;
    }

    // Fare konumunun panel içinde olup olmadığını kontrol et
    if (IsPositionInsidePanel(InAbsoluteCursorPosition))
    {
        // Çizim modu aktifse ve fare panel içindeyse hit testi geçsin
        return true;
    }

    // Panel dışındaysa kesinlikle hit testi geçmesin ki alttaki UI elemanları etkileşime girebilsin
    return false;
}

// OnMouseButtonDown - fare tıklamaları için
FReply SNexxNoteDrawingOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!GraphPanel)
	{
		return FReply::Unhandled();
	}

	// Sketch modu aktifse ve sol fare tuşu ile tıklandıysa
	if (bSketchModeActive && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Çizim başlat
		StartDrawing(MyGeometry, MouseEvent);
		
		// Slate sistemi bu tıklamayı tüketmesi için handled döndürüyoruz
		return FReply::Handled().CaptureMouse(AsShared());
	}
	// Sketch modu aktifse ve sağ fare tuşu ile tıklandıysa
	else if (bSketchModeActive && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Sağ fare tuşuna basıldığında, tuş bırakıldığında sketch modunu devre dışı bırakacağız
		// Burada sadece tıklamayı işaretliyoruz
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FReply SNexxNoteDrawingOverlay::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (!GraphPanel || !bSketchModeActive)
    {
        return FReply::Unhandled();
    }

    // Fare koordinatlarını kaydet (loglar için)
    LastMousePos = MouseEvent.GetScreenSpacePosition();

    // Çizim modunda mıyız ve fare basılı mı kontrol et
    if (bSketchModeActive && bDrawing && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
        {
        // GraphPanel'dan gerekli bilgileri al
            FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
        float ZoomAmount = GraphPanel->GetZoomAmount();
        FVector2D ViewOffset = GraphPanel->GetViewOffset();
        
        // Ekran koordinatından panel koordinatına dönüştür
        FVector2D PanelSpacePosition = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            
        // Panel koordinatından graph koordinatına dönüştür
        FVector2D GraphPosition = (PanelSpacePosition / ZoomAmount) + ViewOffset;

        // Son nokta ile bu nokta arasındaki mesafeyi kontrol et
            if (DrawPoints.Num() > 0)
            {
            // ÖNEMLİ: Zoom seviyesine göre minimum mesafeyi ayarla
            float MinDistanceThreshold = 3.0f / ZoomAmount; // Zoom seviyesine göre mesafe eşiği
            float Distance = FVector2D::Distance(DrawPoints.Last(), GraphPosition);
            
            // Minimum mesafe kontrolü - çok yakın noktaları eklemeyi önle
            if (Distance > MinDistanceThreshold) 
            {
                DrawPoints.Add(GraphPosition);
                
                // Her nokta eklendiğinde görsel güncelleme yap
                Invalidate(EInvalidateWidgetReason::Paint);
            }
        }
        
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

// OnMouseButtonUp - fare tuşu bırakıldığında
FReply SNexxNoteDrawingOverlay::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!GraphPanel)
	{
		return FReply::Unhandled();
	}

	// Sol fare tuşu bırakıldıysa ve çizim modunda isek
	if (bSketchModeActive && bDrawing && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Son noktayı ekle
		FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
		float ZoomAmount = GraphPanel->GetZoomAmount();
		FVector2D ViewOffset = GraphPanel->GetViewOffset();
		
		// Ekran koordinatından panel koordinatına dönüştür
		FVector2D PanelSpacePosition = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		
		// Panel koordinatından graph koordinatına dönüştür
		FVector2D GraphPosition = (PanelSpacePosition / ZoomAmount) + ViewOffset;
		
		// Son noktayı ekle (eğer henüz eklenmemişse)
		if (DrawPoints.Num() > 0)
		{
			float Distance = FVector2D::Distance(DrawPoints.Last(), GraphPosition);
			if (Distance > 2.0f / ZoomAmount)
			{
				DrawPoints.Add(GraphPosition);
			}
		}
		
		// Shift tuşu durumunu kontrol et ve kaydet
		bWasShiftKeyDown = IsShiftKeyDown();
		
		// Shift basılı değilse, çizimi tamamla ve node oluştur
		if (!bWasShiftKeyDown)
		{
			// Çizimi tamamla ve node oluştur
			FinishDrawing();
		}
		
		// Yeniden çizim başlatabileceğimiz bir duruma dönelim
		bDrawing = false;
		
		return FReply::Handled().ReleaseMouseCapture();
	}
	// Sağ fare tuşu ile çizim modunu devre dışı bırakabiliriz
	else if (bSketchModeActive && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Eğer shift ile birikmiş çizim varsa, önce onu tamamla
		if (DrawPoints.Num() >= 2 && bWasShiftKeyDown)
		{
			FinishDrawing();
		}
		
		// Sketch modunu kapat
		SetSketchModeActive(false);
		
		// Delegat ile ana modülü bilgilendir
		if (OnSketchModeDeactivated.IsBound())
		{
			OnSketchModeDeactivated.Execute();
		}
		
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

// Tick fonksiyonu - her frame çalışır, fare konumunu kontrol eder
void SNexxNoteDrawingOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    if (!GraphPanel)
    {
        return;
    }
    
    if (bSketchModeActive)
    {
        // Bilgilendirme textini güncelle
        if (bDrawing)
        {
            // Çizim yapılıyor durumu
            StatusTextBlock->SetText(FText::FromString(TEXT("Drawing mode ACTIVE")));
            StatusTextBlock->SetVisibility(EVisibility::Visible);
            
            // Shift tuşunun durumunu kontrol et
            bool bIsShiftDown = IsShiftKeyDown();
            
            // Shift durumunu güncelle
            if (bIsShiftDown)
            {
                ShiftStateTextBlock->SetVisibility(EVisibility::Visible);
            }
            else
            {
                ShiftStateTextBlock->SetVisibility(EVisibility::Collapsed);
            }
        }
        else
        {
            // Çizim modu aktif durumu
            StatusTextBlock->SetText(FText::FromString(TEXT("Drawing mode ACTIVE")));
            StatusTextBlock->SetVisibility(EVisibility::Visible);
            
            // Shift tuşunun durumunu kontrol et
            bool bIsShiftDown = IsShiftKeyDown();
            
            // Shift durumunu güncelle
            if (bIsShiftDown)
            {
                ShiftStateTextBlock->SetVisibility(EVisibility::Visible);
            }
            else
            {
                ShiftStateTextBlock->SetVisibility(EVisibility::Collapsed);
            }
        }
        
        // Fare konumunu kontrol et
        FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();
        bool bIsMouseCurrentlyInPanel = IsPositionInsidePanel(MousePosition);
        
        // Fare konumu değiştiyse işaretle
        if (bIsMouseInsidePanel != bIsMouseCurrentlyInPanel)
        {
            bIsMouseInsidePanel = bIsMouseCurrentlyInPanel;
            
            // İmleci güncelle
            if (bIsMouseInsidePanel)
            {
                SetCursor(EMouseCursor::Crosshairs);
            }
            else
            {
                SetCursor(EMouseCursor::Default);
                
                // Panel dışına çıkınca ve sol fare tuşu basılı değilse, çizimi iptal et
                if (!bIsMouseLeftButtonDown && bDrawing)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[NexxNote-Overlay] Mouse left panel while drawing, canceling drawing"));
                    bDrawing = false;
                    DrawPoints.Empty();
                    
                    // Widget'ı yeniden çizilmesi için işaretle
                    Invalidate(EInvalidateWidgetReason::Paint);
                }
            }
        }
        
        // ÖNEMLİ: Mouse capture state'ini manuel kontrol et
        // Eğer biz çizim yapmıyorsak ve hala mouse capture sahipsek, serbest bırak
        if (HasMouseCapture() && !bDrawing)
        {
            UE_LOG(LogTemp, Warning, TEXT("[NexxNote-Overlay] Tick - Releasing mouse capture as we're not drawing"));
            FSlateApplication::Get().ReleaseAllPointerCapture();
        }
        
        // Mouse tuşu basılı olmadığını tespit et (farenin tuşu dışarıda bırakılabilir)
        if (bIsMouseLeftButtonDown && !FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
        {
            bIsMouseLeftButtonDown = false;
            
            // Çizim durumunu güncelle
            if (bDrawing)
            {
                UE_LOG(LogTemp, Warning, TEXT("[NexxNote-Overlay] Left mouse button released outside of mouse events, finishing drawing"));
                
                // Eğer yeterli nokta varsa çizimi tamamla
                if (DrawPoints.Num() >= 2)
                {
                    // Shift tuşu durumunu kontrol et
                    bool bIsShiftDown = IsShiftKeyDown();
                    
                    // Eğer Shift basılı değilse çizimi tamamla
                    if (!bIsShiftDown)
                    {
                        FinishDrawing();
                        
                        if (OnSketchCompleted.IsBound())
                        {
                            OnSketchCompleted.Execute(DrawPoints);
                        }
                    }
                    else
                    {
                        // Shift basılıysa, çizimi tamamlama ama bDrawing'i false yap
                        bDrawing = false;
                        // Noktaları temizleme, çünkü bir sonraki çizimde devam edeceğiz
                    }
                }
                else
                {
                    // Yetersiz nokta varsa temizle
                    if (!IsShiftKeyDown())
                    {
                        DrawPoints.Empty();
                    }
                    bDrawing = false;
                }
                
                // Mouse yakalamayı serbest bırak
                if (HasMouseCapture())
                {
                    UE_LOG(LogTemp, Warning, TEXT("[NexxNote-Overlay] Releasing mouse capture after drawing"));
                    FSlateApplication::Get().ReleaseAllPointerCapture();
                }
                
                // Widget'ı yeniden çizilmesi için işaretle
                Invalidate(EInvalidateWidgetReason::Paint);
            }
        }
        
        // Eğer shift daha önce basılıydı ama şimdi basılı değilse ve noktalar varsa
        if (bWasShiftKeyDown && !IsShiftKeyDown() && DrawPoints.Num() >= 2 && !bDrawing)
        {
            // Shift bırakıldığında birikmiş çizimi tamamla
            FinishDrawing();
            bWasShiftKeyDown = false;
        }
    }
    else
    {
        // Çizim modu aktif değilse bilgilendirme textlerini gizle
        StatusTextBlock->SetVisibility(EVisibility::Collapsed);
        ShiftStateTextBlock->SetVisibility(EVisibility::Collapsed);
    }
}

void SNexxNoteDrawingOverlay::SetSketchModeActive(bool bActive)
{
    // Eğer durum değişmiyorsa gereksiz işlemler yapmayalım
    if (bSketchModeActive == bActive)
    {
        return;
    }
    
    // Sketch modunu aktif etme
    if (bActive)
    {
        // Not modunu devre dışı bırak
        bNoteModeActive = false;
        
        // Widget'ı görünür yap
        SetVisibility(EVisibility::Visible);
        
        // İmleci ayarla
        SetCursor(EMouseCursor::Crosshairs);
        
        // Sketch modunu aktifleştir
        bSketchModeActive = true;
        
        // Bilgilendirme textlerini güncelle
        StatusTextBlock->SetText(FText::FromString(TEXT("Drawing mode ACTIVE")));
        StatusTextBlock->SetVisibility(EVisibility::Visible);
        
        // Shift tuşunun durumunu kontrol et
        bool bIsShiftDown = IsShiftKeyDown();
        if (bIsShiftDown)
        {
            ShiftStateTextBlock->SetVisibility(EVisibility::Visible);
        }
        else
        {
            ShiftStateTextBlock->SetVisibility(EVisibility::Collapsed);
        }
        
        // ÖNEMLİ: Sketch modu aktifleştirildiğinde çizim durumu her zaman sıfırlanır
        bDrawing = false;
        bIsMouseLeftButtonDown = false;
        DrawPoints.Empty();
        StrokeBreakIndices.Empty();
    }
    // Sketch modunu deaktif etme
    else
    {
        // Mevcut çizim varsa iptal et
        if (bDrawing)
        {
            UE_LOG(LogTemp, Warning, TEXT("[NexxNote-Overlay] Sketch mode deactivated while drawing, cancelling drawing"));
            bDrawing = false;
            bIsMouseLeftButtonDown = false;
            DrawPoints.Empty();
            StrokeBreakIndices.Empty();
        }
        
        // Fare yakalamayı serbest bırak
        if (HasMouseCapture())
        {
            UE_LOG(LogTemp, Warning, TEXT("[NexxNote-Overlay] Releasing mouse capture when deactivating sketch mode"));
            FSlateApplication::Get().ReleaseAllPointerCapture();
        }
        
        // Widget'ı gizle
        SetVisibility(EVisibility::Collapsed);
        
        // Bilgilendirme textlerini gizle
        StatusTextBlock->SetVisibility(EVisibility::Collapsed);
        ShiftStateTextBlock->SetVisibility(EVisibility::Collapsed);
        
        // İmleci normal olarak ayarla
        SetCursor(EMouseCursor::Default);
        
        // GraphPanel'in imleç tipini de sıfırla
        if (GraphPanel)
        {
            GraphPanel->SetCursor(EMouseCursor::Default);
        }
        
        // Sketch modunu deaktif et
        bSketchModeActive = false;
        
        // Slate uygulamasının fare işleyişini sıfırla
        if (FSlateApplication::IsInitialized())
        {
                        FSlateApplication::Get().ResetToDefaultInputSettings();
            
            // Platform imlecini manuel olarak sıfırla
            TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();
            if (PlatformCursor.IsValid())
            {
                PlatformCursor->SetType(EMouseCursor::Default);
            }
        }
    }
}

void SNexxNoteDrawingOverlay::SetNoteModeActive(bool bActive)
{
    // Not modu durumunu güncelle
    bNoteModeActive = bActive;
    
    // Sketch ve Not modu aynı anda aktif olmamalı
    if (bActive)
    {
        bSketchModeActive = false;
        
        // Not modunda Widget'ı görünür yapmayalım ve etkileşime girmesin
        SetVisibility(EVisibility::HitTestInvisible);
    }
}

void SNexxNoteDrawingOverlay::StartDrawing(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (!GraphPanel) 
    {
        return;
    }
    
    bDrawing = true;
    
    // Bilgilendirme textlerini güncelle
    StatusTextBlock->SetText(FText::FromString(TEXT("Drawing mode ACTIVE")));
    StatusTextBlock->SetVisibility(EVisibility::Visible);
    
    // Shift tuşunun durumunu kontrol et
    bool bIsShiftDown = IsShiftKeyDown();
    if (bIsShiftDown)
    {
        ShiftStateTextBlock->SetVisibility(EVisibility::Visible);
    }
    else
    {
        ShiftStateTextBlock->SetVisibility(EVisibility::Collapsed);
    }
    
    // ÖNEMLİ DEĞİŞİKLİK: Shift basılıysa ve önceki noktalar varsa
    if (bIsShiftDown && DrawPoints.Num() > 0)
    {
        // Yeni çizim parçası başlatıyoruz - mevcut DrawPoints sayısını kaydet
        // Bu, bu indeksten sonraki noktaların yeni bir çizim parçası olduğunu belirtir
        StrokeBreakIndices.Add(DrawPoints.Num());
    }
    else
    {
        // Shift basılı değilse, tüm noktaları ve parça indekslerini temizle
        DrawPoints.Empty();
        StrokeBreakIndices.Empty();
    }
    
    // Ekran koordinatından GraphPanel'ın yerel koordinatlarına dönüştürme
    FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
    FVector2D ScreenSpacePosition = MouseEvent.GetScreenSpacePosition();
    FVector2D PanelSpacePosition = PanelGeometry.AbsoluteToLocal(ScreenSpacePosition);
    
    // GraphPanel'ın zoom ve pan değerlerini al
    float ZoomAmount = GraphPanel->GetZoomAmount();
    FVector2D ViewOffset = GraphPanel->GetViewOffset();
    
    // Ekran pozisyonundan Graph koordinatlarına dönüştürme
    FVector2D GraphPosition = (PanelSpacePosition / ZoomAmount) + ViewOffset;
    
    // İlk noktayı ekle - GRAPH koordinatlarında sakla
    DrawPoints.Add(GraphPosition);
    
    // ÖNEMLİ: Çizim başladığında görsel güncellemeyi zorla
    MarkPrepassAsDirty();
    Invalidate(EInvalidateWidgetReason::Paint);
    
    // GraphPanel'i de zorunlu olarak yenileyelim
    if (GraphPanel)
    {
        GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
        GraphPanel->Invalidate(EInvalidateWidgetReason::Paint);
        
        // Parent pencereyi de yenileyelim
        TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(GraphPanel->AsShared());
        if (ParentWindow.IsValid())
        {
            ParentWindow->Invalidate(EInvalidateWidgetReason::Paint);
        }
    }
}

void SNexxNoteDrawingOverlay::FinishDrawing()
{
    // Bilgilendirme textlerini güncelle
    StatusTextBlock->SetText(FText::FromString(TEXT("Drawing mode ACTIVE")));
    
    // Shift tuşunun durumunu kontrol et
    bool bIsShiftDown = IsShiftKeyDown();
    if (bIsShiftDown)
    {
        ShiftStateTextBlock->SetVisibility(EVisibility::Visible);
    }
    else
    {
        ShiftStateTextBlock->SetVisibility(EVisibility::Collapsed);
    }
    
    if (DrawPoints.Num() < 2)
    {
        // Geçerli bir çizim için en az 2 nokta gerekiyor
        DrawPoints.Empty();  // Temizlik yapalım
        return;
    }
    
    // Orijinal DrawPoints'i kopyala - bu sayede işlem sırasında çizim kaybolmaz
    TArray<FVector2D> TempDrawPoints = DrawPoints;
    TArray<int32> TempBreakIndices = StrokeBreakIndices;

    // Her segmenti ayrı ayrı işlemek için önce segmentlere ayır
    TArray<TArray<FVector2D>> SegmentPoints;
    TArray<int32> RelativeBreakIndices;
    
    // İlk segmenti başlat
    int32 StartIndex = 0;
    
    // StrokeBreakIndices kullanarak mevcut çizimi parçalara ayır
    for (int32 BreakIndex = 0; BreakIndex <= TempBreakIndices.Num(); BreakIndex++)
    {
        int32 EndIndex;
        
        // Son segment için veya hiç kesme indeksi yoksa
        if (BreakIndex == TempBreakIndices.Num())
        {
            EndIndex = TempDrawPoints.Num();
        }
        else
        {
            EndIndex = TempBreakIndices[BreakIndex];
        }
        
        // Bu segment geçerli mi? (en az 2 nokta içeriyor mu?)
        if (EndIndex - StartIndex >= 2)
        {
            // Bu segment için yeni bir nokta dizisi oluştur
            TArray<FVector2D> CurrentSegment;
            
            // Bu segmentin noktalarını ekle
            for (int32 i = StartIndex; i < EndIndex; i++)
            {
                CurrentSegment.Add(TempDrawPoints[i]);
            }
            
            // Segment listesine ekle
            SegmentPoints.Add(CurrentSegment);
        }
        
        // Sonraki segment için başlangıç indeksini güncelle
        StartIndex = EndIndex;
    }
    
    // Hiç geçerli segment yoksa çık - orijinal çizimi koruyalım
    if (SegmentPoints.Num() == 0)
    {
        return;
    }
    
    // Tüm segmentleri işledikten sonra birleştirilecek nihai noktalar
    TArray<FVector2D> AllProcessedPoints;
    
    // Her segmenti ayrı ayrı optimize et ve smooth'la
    for (int32 SegmentIndex = 0; SegmentIndex < SegmentPoints.Num(); SegmentIndex++)
    {
        // Bu segment için optimize edilmiş noktaları hesapla
        TArray<FVector2D> OptimizedSegment;
        OptimizeDrawPoints(SegmentPoints[SegmentIndex], OptimizedSegment);
        
        // Yetersiz nokta kalmışsa bu segmenti atla
        if (OptimizedSegment.Num() < 2)
        {
            continue;
        }
        
        // Bu segment için yumuşatılmış noktaları hesapla
        TArray<FVector2D> SmoothedSegment;
        SmoothDrawPoints(OptimizedSegment, SmoothedSegment, 3);
        
        // Eğer bu ilk segment değilse, bir kesme noktası ekle
        if (AllProcessedPoints.Num() > 0)
        {
            // Kesme noktası ekle - bu noktadan sonraki noktalar yeni segmente ait
            RelativeBreakIndices.Add(AllProcessedPoints.Num());
        }
        
        // İşlenmiş segmenti nihai listeye ekle
        AllProcessedPoints.Append(SmoothedSegment);
    }
    
    // İşlenmiş nokta sayısını kontrol et
    if (AllProcessedPoints.Num() < 2)
    {
        return;
    }

    // Graph üzerinde kalıcı bir SketchNode oluştur
    if (GraphPanel && GraphPanel->GetGraphObj())
    {
        UEdGraph* Graph = GraphPanel->GetGraphObj();
        
        // Transaction başlat - bu işlemi geri alınabilir yap
        FScopedTransaction Transaction(LOCTEXT("NexxNoteCreateSketch", "Create Sketch"));
        
        // Graph koordinatlarında min/max değerleri hesapla
        FVector2D MinGraphPoint(FLT_MAX, FLT_MAX);
        FVector2D MaxGraphPoint(-FLT_MAX, -FLT_MAX);

        for (const FVector2D& Point : AllProcessedPoints)
        {
            MinGraphPoint.X = FMath::Min(MinGraphPoint.X, Point.X);
            MinGraphPoint.Y = FMath::Min(MinGraphPoint.Y, Point.Y);
            MaxGraphPoint.X = FMath::Max(MaxGraphPoint.X, Point.X);
            MaxGraphPoint.Y = FMath::Max(MaxGraphPoint.Y, Point.Y);
        }
        
        // Graph içindeki merkez noktayı ve boyutları hesapla
        FVector2D GraphCenter = (MinGraphPoint + MaxGraphPoint) * 0.5f;
        FVector2D GraphSize = MaxGraphPoint - MinGraphPoint;
        
        // Zoom faktörünü al
        float ZoomAmount = GraphPanel->GetZoomAmount();
        
        // Padding değerini ayarla - Zoom faktöründen bağımsız olarak sabit kalmalı
        float Padding = 15.0f;
        
        // Node'un sol üst köşesini hesapla
        FVector2D NodePos = MinGraphPoint - FVector2D(Padding, Padding);
        
        // Node'un boyutunu hesapla (çizim boyutu + padding)
        FVector2D NodeSize = GraphSize + FVector2D(Padding * 2.0f, Padding * 2.0f);
        
        // Yeni bir SketchNode oluştur
        UNexxSketchNode* NewNode = NewObject<UNexxSketchNode>(Graph);
        if (NewNode)
        {
            // Düğümü graph'a eklemeden önce modifiye edildiğini işaretle
            NewNode->Modify();
            Graph->Modify();
            
            // Graph'a node'u ekle
            Graph->AddNode(NewNode, true);
            
            // Node pozisyonunu ayarla
            NewNode->NodePosX = NodePos.X;
            NewNode->NodePosY = NodePos.Y;
            
            // Çizim noktalarını node pozisyonuna göre relatif hale getir
            TArray<FVector2D> RelativePoints;
            for (const FVector2D& GraphPoint : AllProcessedPoints)
            {
                // Node sol üst köşesine göre göreceli konum
                RelativePoints.Add(GraphPoint - NodePos);
            }
            
            // Segment bilgisiyle birlikte çizim verilerini ayarla
            NewNode->SetSketchData(RelativePoints, RelativeBreakIndices, DrawColor, DrawThickness);
            
            // Grafiği güncelle ve Blueprinti değiştirilmiş olarak işaretle
            if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
            {
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            }
            
            // Node'u yenile ve graph paneli güncelle
            GraphPanel->RefreshNode(*NewNode);
            GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
            
            // Blueprint editor'ü ve düğüm factory'i notify et
            TSharedPtr<SGraphNode> NodeWidget = GraphPanel->GetNodeWidgetFromGuid(NewNode->NodeGuid);
            if (NodeWidget.IsValid())
            {
                NodeWidget->UpdateGraphNode();
            }
            
            // Hemen temizlemek yerine, bir frame gecikmeli temizle
            FTimerHandle TempTimerHandle;
            GEditor->GetTimerManager()->SetTimer(TempTimerHandle, [this]()
            {
                DrawPoints.Empty();
                StrokeBreakIndices.Empty();
                bDrawing = false;
                
                Invalidate(EInvalidateWidgetReason::Paint);

            }, 0.02f, false); // 1 frame sonra temizle (0.02s gibi)
        }
    }
    else
    {
        // Graph paneli yoksa da çizimi temizle - bu durumda gecikmeye gerek yok
        DrawPoints.Empty();
        StrokeBreakIndices.Empty();
        bDrawing = false;
    }
}

FVector2D SNexxNoteDrawingOverlay::GraphCoordToPanelCoord(const FVector2D& GraphCoord) const
{
    // Bu fonksiyon kaldırıldı - doğrudan mouse koordinatlarını kullanıyoruz
    return GraphCoord;
}

FVector2D SNexxNoteDrawingOverlay::PanelCoordToGraphCoord(const FVector2D& PanelCoord) const
{
    // Bu fonksiyon kaldırıldı - doğrudan mouse koordinatlarını kullanıyoruz
    return PanelCoord;
}

void SNexxNoteDrawingOverlay::SetDrawingColor(const FLinearColor& NewColor)
{
    // Eğer renk farklıysa güncelle
    if (!DrawColor.Equals(NewColor))
    {
        DrawColor = NewColor;
        
        // Renk değiştiği bilgisini yayınla
        if (OnDrawingColorChanged.IsBound())
        {
            OnDrawingColorChanged.Execute(DrawColor);
        }
        
        // Grafik arayüzünü güncelle
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SNexxNoteDrawingOverlay::SetDrawingThickness(float NewThickness)
{
    // Geçersiz kalınlık değerlerini kontrol et
    NewThickness = FMath::Clamp(NewThickness, 0.5f, 10.0f);
    
    // Eğer kalınlık farklıysa güncelle
    if (DrawThickness != NewThickness)
    {
        DrawThickness = NewThickness;
        
        // Kalınlık değiştiği bilgisini yayınla
        if (OnDrawingThicknessChanged.IsBound())
        {
            OnDrawingThicknessChanged.Execute(DrawThickness);
        }
        
        // Grafik arayüzünü güncelle
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

// FindAllWidgetsRecursive fonksiyonu artık NexxNoteHelpers namespace'inde tanımlı

// Panel koordinatlarını Graph koordinatlarına dönüştüren yardımcı fonksiyon
FVector2D SNexxNoteDrawingOverlay::PanelToGraphCoord(const FVector2D& PanelCoord, float ZoomFactor, const FVector2D& ViewOff) const
{
    // ÖNEMLİ: Bu fonksiyonda kritik değişiklikler yapıyoruz
    // Zoom faktörü çok önemli, çünkü zoom değiştikçe sağ-sol veya yukarı-aşağı kayma yaşanıyor
    
    // Düzeltilmiş formül: Doğrudan panel koordinatlarını Graph koordinatlarına dönüştürür
    // Bu formül tüm zoom seviyelerinde tutarlı bir şekilde çalışır
    return (PanelCoord / ZoomFactor) + ViewOff;
}

// Shift tuşunun basılı olup olmadığını kontrol et
bool SNexxNoteDrawingOverlay::IsShiftKeyDown() const
{
    FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
    return KeyState.IsShiftDown();
}

#undef LOCTEXT_NAMESPACE 
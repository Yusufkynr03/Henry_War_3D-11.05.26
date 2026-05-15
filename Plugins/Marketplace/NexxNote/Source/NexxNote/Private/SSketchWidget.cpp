// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "SSketchWidget.h"
#include "SGraphPanel.h"
#include "Rendering/DrawElements.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_Variable.h"
#include "SketchNode.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SSketchWidget::Construct(const FArguments& InArgs)
{
	GraphPanel = InArgs._GraphPanel;
	bSketchModeActive = false;
	bNoteModeActive = false;
	bIsDrawing = false;
	CurrentSketchColor = FLinearColor::White;
	CurrentSketchThickness = 2.0f;

	// Slate widget'ı tamamen şeffaf ama mouse etkileşimlerini yakalayan bir panel olarak tanımla
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
			.Padding(0)
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)) // Tamamen şeffaf
			.Visibility(this, &SSketchWidget::GetSketchVisibility)
		]
	];

	// UE5.4'te başlangıçta görünür ayarlayalım ama HitTest özelliğini kapalı tutalım
	// Bu, widget'ın paint edilmesini sağlar ancak mouse olaylarını yakalamaz
	SetVisibility(EVisibility::HitTestInvisible);
	
	// UE5.4'te tick ve focus için ayarları yapalım
	SetCanTick(true);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EVisibility SSketchWidget::GetSketchVisibility() const
{
	return bSketchModeActive ? EVisibility::Visible : EVisibility::HitTestInvisible;
}

FReply SSketchWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Sketch modu aktif mi kontrol et ve log çıktısı ver
	UE_LOG(LogTemp, Warning, TEXT("OnMouseButtonDown: Sketch Mode Active: %s, Button: %s, Visibility: %s"), 
		bSketchModeActive ? TEXT("TRUE") : TEXT("FALSE"),
		*MouseEvent.GetEffectingButton().ToString(),
		*GetVisibility().ToString());
	
	// Sketch modu aktifse ve sol tıklama yapıldıysa çizmeye başla
	if (bSketchModeActive && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UE_LOG(LogTemp, Warning, TEXT("Starting sketch drawing!"));
		UE_LOG(LogTemp, Warning, TEXT("Widget Class: %s"), *GetTypeAsString()); 
		UE_LOG(LogTemp, Warning, TEXT("Current Position: (%f, %f)"), 
			MyGeometry.AbsolutePosition.X, MyGeometry.AbsolutePosition.Y);
		UE_LOG(LogTemp, Warning, TEXT("Size: (%f, %f)"), 
			MyGeometry.GetLocalSize().X, MyGeometry.GetLocalSize().Y);
		
		// Start drawing
		bIsDrawing = true;
		ClearCurrentSketch();
		
		// Add the first point - GraphPanel koordinatlarına dönüştürelim
		if (GraphPanel)
		{
			// Fare pozisyonunu graph panel'in lokal koordinatlarına dönüştürelim
			FVector2D LocalPosition;
			
			// Mouse konumunu alalım - önce ekran koordinatlarını, sonra widget lokal koordinatlarını
			FVector2D MousePos = MouseEvent.GetScreenSpacePosition();
			UE_LOG(LogTemp, Warning, TEXT("Mouse Screen Position: %s"), *MousePos.ToString());
			
			// GraphPanel'in geometrisini kullanarak dönüşüm yapabiliriz
			FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
			LocalPosition = PanelGeometry.AbsoluteToLocal(MousePos);
			UE_LOG(LogTemp, Warning, TEXT("Mouse Local Position on GraphPanel: %s"), *LocalPosition.ToString());
			
			// Dönüştürülmüş pozisyonu sketch points'e ekleyelim
			AddSketchPoint(LocalPosition);
		}
		else
		{
			// GraphPanel yoksa, doğrudan widget içindeki lokal koordinatları kullanalım
			FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			AddSketchPoint(LocalPosition);
			UE_LOG(LogTemp, Warning, TEXT("Local position (No GraphPanel): %s"), *LocalPosition.ToString());
		}
		
		// Widget'ı tekrar çizdirmek için invalidate et
		Invalidate(EInvalidateWidgetReason::Layout);
		
		// GraphPanel'i de güncelleyelim
		if (GraphPanel)
		{
			GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
		}
		
		// Capture the mouse so we get mouse moves
		return FReply::Handled().CaptureMouse(SharedThis(this)).UseHighPrecisionMouseMovement(SharedThis(this));
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Right click cancels sketch mode
		if (bSketchModeActive)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cancelling sketch mode with right click"));
			bSketchModeActive = false;
			ClearCurrentSketch();
			SetVisibility(EVisibility::HitTestInvisible);
			SetCursor(EMouseCursor::Default);
			
			// GraphPanel'i de güncelleyelim
			if (GraphPanel)
			{
				GraphPanel->SetCursor(EMouseCursor::Default);
				GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
			}
			
			return FReply::Handled();
		}
	}

	// Sketch modu aktif değilse eventi geçirebiliriz
	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SSketchWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("OnMouseButtonUp: Drawing=%s, Button=%s"), 
		bIsDrawing ? TEXT("TRUE") : TEXT("FALSE"),
		*MouseEvent.GetEffectingButton().ToString());
		
	if (bIsDrawing && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Finish drawing
		bIsDrawing = false;
		
		// Add final point
		if (GraphPanel)
		{
			// GraphPanel koordinat sistemine dönüştür
			FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
			FVector2D LocalPosition = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			AddSketchPoint(LocalPosition);
		}
		else
		{
			// Doğrudan widget içindeki koordinatları kullan
			FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			AddSketchPoint(LocalPosition);
		}
		
		// Çizimi bitirdiğimizi loglayalım
		UE_LOG(LogTemp, Warning, TEXT("Finished sketch with %d points"), CurrentSketchPoints.Num());
		
		// Create the sketch node
		FinishSketch();
		
		// Widget'ı ve panel'i tekrar çizdirmek için invalidate et
		Invalidate(EInvalidateWidgetReason::Layout);
		if (GraphPanel)
		{
			GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
		}
		
		// Release mouse capture
		return FReply::Handled().ReleaseMouseCapture();
	}

	return SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SSketchWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Çizim modunda ise noktaları ekleyelim
	if (bIsDrawing && bSketchModeActive)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Mouse move while drawing: %s"), *MouseEvent.GetScreenSpacePosition().ToString());
		
		// Add a point to the sketch
		if (GraphPanel)
		{
			// GraphPanel koordinat sistemine dönüştür
			FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
			FVector2D LocalPosition = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			AddSketchPoint(LocalPosition);
			
			// Her 10 noktada bir log
			if (CurrentSketchPoints.Num() % 10 == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("Added point #%d at %s"), 
					CurrentSketchPoints.Num(), *LocalPosition.ToString());
			}
		}
		else
		{
			// Doğrudan widget içindeki koordinatları kullan
			FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			AddSketchPoint(LocalPosition);
		}
		
		// Widget'ı ve panel'i tekrar çizdirmek için invalidate et
		Invalidate(EInvalidateWidgetReason::Layout);
		if (GraphPanel && (CurrentSketchPoints.Num() % 5 == 0)) // Perforans için her 5 noktada bir güncelle
		{
			GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
		}
		
		return FReply::Handled();
	}

	return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
}

int32 SSketchWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// İlk önce temel sınıfın OnPaint fonksiyonunu çağıralım
	int32 CurrentLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	
	// Sketch modu aktif değilse hiçbir şey çizme
	if (!bSketchModeActive)
	{
		return CurrentLayer;
	}
	
	// Her 300 framede bir log yazdır (UI spam önlemek için)
	static int32 FrameCounter = 0;
	FrameCounter++;
	if (FrameCounter % 300 == 0 || CurrentSketchPoints.Num() > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("OnPaint - Sketch Mode Active: TRUE, Points: %d"), CurrentSketchPoints.Num());
		UE_LOG(LogTemp, Display, TEXT("Geometry Size: (%f, %f), Position: (%f, %f)"), 
			AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y,
			AllottedGeometry.AbsolutePosition.X, AllottedGeometry.AbsolutePosition.Y);
		FrameCounter = 0;
	}
	
	// Draw a small indicator in top right to show sketch mode is on
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		CurrentLayer++,
		// Sağ üst köşeye 20x20 kırmızı bir kare çizelim
		AllottedGeometry.ToPaintGeometry(FVector2f(20, 20), FSlateLayoutTransform(FVector2f(AllottedGeometry.GetLocalSize().X - 30, 10.0f))),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Red
	);
	
	// Sketch noktaları varsa çizim yap
	if (CurrentSketchPoints.Num() > 0)
	{
		// Log çizim bilgisini
		UE_LOG(LogTemp, Display, TEXT("Drawing sketch with %d points"), CurrentSketchPoints.Num());
		
		// Her şekilde tüm noktaları logla
		for (int32 i = 0; i < CurrentSketchPoints.Num(); ++i)
		{
			UE_LOG(LogTemp, Display, TEXT("  Point[%d]: %s"), i, *CurrentSketchPoints[i].ToString());
		}
		
		// Sketch drawing mode - çizgiler oluştur
		if (CurrentSketchPoints.Num() > 1)
		{
			for (int32 Index = 0; Index < CurrentSketchPoints.Num() - 1; ++Index)
			{
				FVector2D Point1 = CurrentSketchPoints[Index];
				FVector2D Point2 = CurrentSketchPoints[Index + 1];
				
				// GraphPanel varsa ve geçerliyse, koordinatları widget'a göre ayarla
				if (GraphPanel != nullptr)
				{
					// Graph panel'in geometrisini kullanarak, çizim için doğru koordinatları hesaplayalım
					FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
					
					// Noktalar panel koordinatlarında olduğundan, widget koordinatlarına dönüştürelim
					// Bu işlem, panel içindeki pozisyonları widget içindeki pozisyonlara çevirir
					FVector2D LocalPoint1 = AllottedGeometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(Point1));
					FVector2D LocalPoint2 = AllottedGeometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(Point2));
					
					// Çizgi koordinatlarını logla
					//UE_LOG(LogTemp, Display, TEXT("Line from %s to %s"), *LocalPoint1.ToString(), *LocalPoint2.ToString());
					
					// Desenli çizgi oluştur (daha farkedilir olması için)
					TArray<FVector2D> LinePoints;
					LinePoints.Add(LocalPoint1);
					LinePoints.Add(LocalPoint2);
					
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						CurrentLayer,
						AllottedGeometry.ToPaintGeometry(),
						LinePoints,
						ESlateDrawEffect::None,
						FLinearColor::Yellow, // Sarı renk
						true, // Kalın çizgi
						3.0f // Çizgi kalınlığı
					);
				}
				else
				{
					// GraphPanel yoksa doğrudan çizim yap
					TArray<FVector2D> LinePoints;
					LinePoints.Add(Point1);
					LinePoints.Add(Point2);
					
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						CurrentLayer,
						AllottedGeometry.ToPaintGeometry(),
						LinePoints,
						ESlateDrawEffect::None,
						FLinearColor::Yellow,
						true,
						3.0f
					);
				}
			}
		}
		
		// Her nokta için küçük bir daire çizelim
		for (const FVector2D& Point : CurrentSketchPoints)
		{
			FVector2D LocalPoint = Point;
			
			// GraphPanel varsa ve geçerliyse, koordinatları widget'a göre ayarla
			if (GraphPanel != nullptr)
			{
				FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
				LocalPoint = AllottedGeometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(Point));
			}
			
			// Küçük bir kırmızı daire çizelim
			const float PointSize = 8.0f;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				CurrentLayer,
				AllottedGeometry.ToPaintGeometry(FVector2f(PointSize, PointSize), FSlateLayoutTransform(FVector2f(LocalPoint.X - PointSize/2.0f, LocalPoint.Y - PointSize/2.0f))),
				FAppStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor::Red
			);
		}
	}
	
	CurrentLayer++;
	return CurrentLayer;
}

void SSketchWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	// Mod değiştikçe konsola log yazalım
	static bool bPrevSketchModeActive = false;
	static bool bPrevNoteModeActive = false;
	
	if (bPrevSketchModeActive != bSketchModeActive || bPrevNoteModeActive != bNoteModeActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mode changed - Sketch: %s, Note: %s"), 
			bSketchModeActive ? TEXT("ACTIVE") : TEXT("inactive"),
			bNoteModeActive ? TEXT("ACTIVE") : TEXT("inactive"));
		
		bPrevSketchModeActive = bSketchModeActive;
		bPrevNoteModeActive = bNoteModeActive;
	}
	
	// Update visibility based on mode
	if (bSketchModeActive || bNoteModeActive)
	{
		// Sketch veya Note modu aktifse, mouse olaylarını yakalayabilmek için Visible olmalı
		if (GetVisibility() != EVisibility::Visible)
		{
			SetVisibility(EVisibility::Visible);
			UE_LOG(LogTemp, Warning, TEXT("Setting widget to Visible to capture mouse events"));
		}
	}
	else
	{
		// Sketch/Note modu aktif değilse, mouse olaylarını yakalamadan sadece mevcudiyeti korumalı
		if (GetVisibility() != EVisibility::HitTestInvisible)
		{
			SetVisibility(EVisibility::HitTestInvisible);
			UE_LOG(LogTemp, Warning, TEXT("Setting widget to HitTestInvisible"));
		}
	}
}

void SSketchWidget::SetSketchModeActive(bool bActive)
{
	// Eğer zaten aynı durumdaysa değişiklik yapmaya gerek yok
	if (bSketchModeActive == bActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("SketchMode already %s, not changing"), bActive ? TEXT("ACTIVE") : TEXT("inactive"));
		return;
	}
	
	bSketchModeActive = bActive;
	
	// Mod değişimini log olarak yazdır
	UE_LOG(LogTemp, Warning, TEXT("SSketchWidget::SetSketchModeActive(%s)"), bActive ? TEXT("TRUE") : TEXT("FALSE"));
	
	if (bSketchModeActive)
	{
		// Disable note mode if sketch mode is enabled
		bNoteModeActive = false;
		
		// Make widget visible to capture input
		SetVisibility(EVisibility::Visible);
		
		// Aktif olduğunda fare imlecini değiştir
		SetCursor(EMouseCursor::Crosshairs);
		
		// Aktif durum için konsola log yazalım
		UE_LOG(LogTemp, Warning, TEXT("SKETCH MODE ACTIVE! Mouse interactions should be captured"));
		
		// Çizim modunu görsel olarak göstermek için widget'ı yeniden çiz
		if (GraphPanel)
		{
			GraphPanel->SetCursor(EMouseCursor::Crosshairs);
			GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
		}
		
		// Widget'ı yeniden çiz
		Invalidate(EInvalidateWidgetReason::Layout);
		
		// UE5.4'te dikkat: Widget görünürlüğünü Visible olarak ayarlamak kritik
		UE_LOG(LogTemp, Warning, TEXT("Current widget visibility: %s"), *GetVisibility().ToString());
	}
	else
	{
		// Clear any in-progress sketch
		ClearCurrentSketch();
		
		// Reset cursor
		SetCursor(EMouseCursor::Default);
		
		// If both modes are inactive, make invisible
		if (!bNoteModeActive)
		{
			SetVisibility(EVisibility::HitTestInvisible);
		}
		
		// Parent widget'ı güncelleyelim
		if (GraphPanel)
		{
			GraphPanel->SetCursor(EMouseCursor::Default);
			GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
		}
		
		// Widget'ı yeniden çiz
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SSketchWidget::SetNoteModeActive(bool bActive)
{
	bNoteModeActive = bActive;
	
	if (bNoteModeActive)
	{
		// Disable sketch mode if note mode is enabled
		bSketchModeActive = false;
		ClearCurrentSketch();
		
		// Make widget visible to capture input
		SetVisibility(EVisibility::Visible);
	}
	else
	{
		// If both modes are inactive, make invisible
		if (!bSketchModeActive)
		{
			SetVisibility(EVisibility::HitTestInvisible);
		}
	}
}

void SSketchWidget::FinishSketch()
{
	if (CurrentSketchPoints.Num() < 2)
	{
		// Need at least 2 points to create a sketch
		ClearCurrentSketch();
		return;
	}

	// Create a new sketch node in the graph
	if (GraphPanel && GraphPanel->GetGraphObj())
	{
		UEdGraph* Graph = GraphPanel->GetGraphObj();
		
		// Calculate a good position for the node - center of sketch
		FVector2D NodePos(0, 0);
		for (const FVector2D& Point : CurrentSketchPoints)
		{
			NodePos += Point;
		}
		if (CurrentSketchPoints.Num() > 0)
		{
			NodePos /= CurrentSketchPoints.Num();
		}
		
		// Çizim konumlarını graph koordinatlarına çevirelim
		FVector2D GraphPos = GraphPanel->PanelCoordToGraphCoord(NodePos);
		
		// Create the sketch node
		UNexxSketchNode* NewNode = NewObject<UNexxSketchNode>(Graph);
		Graph->AddNode(NewNode, true);
		
		// Set node position
		NewNode->NodePosX = GraphPos.X;
		NewNode->NodePosY = GraphPos.Y;
		
		// Set the sketch data with all points relative to node position
		TArray<FVector2D> RelativePoints;
		for (const FVector2D& Point : CurrentSketchPoints)
		{
			RelativePoints.Add(Point - NodePos);
		}
		
		NewNode->SetSketchData(RelativePoints, CurrentSketchColor, CurrentSketchThickness);
		
		// Clear the current sketch
		ClearCurrentSketch();
		
		// Notify the graph panel that the graph has changed
		if (GraphPanel)
		{
			// GraphPanel->Update() yerine daha seçici bir güncelleme yapalım
			
			// Önce tüm eskimiş widget'ların temizlenmesini sağlayalım
			GraphPanel->PurgeVisualRepresentation();
			
			// Node'u özel olarak güncelleyelim
			GraphPanel->RefreshNode(*NewNode);
			
			// Tüm graph'ı güncelle
			GraphPanel->Invalidate(EInvalidateWidgetReason::Layout);
		}
	}
}

void SSketchWidget::AddSketchPoint(const FVector2D& Point)
{
	// Don't add duplicate points that are too close together
	if (CurrentSketchPoints.Num() > 0)
	{
		const FVector2D& LastPoint = CurrentSketchPoints.Last();
		float Distance = FVector2D::Distance(LastPoint, Point);
		
		// Skip if the point is too close to the last one (to avoid too many points)
		if (Distance < 5.0f)
		{
			return;
		}
	}
	
	// Log yeni nokta
	UE_LOG(LogTemp, Verbose, TEXT("Adding sketch point: %s"), *Point.ToString());
	
	CurrentSketchPoints.Add(Point);
	
	// Noktaların sayısını kontrol et
	if (CurrentSketchPoints.Num() > 1000)
	{
		// Çok fazla nokta varsa, performans için en eski noktaları çıkar
		UE_LOG(LogTemp, Warning, TEXT("Too many sketch points, removing oldest points"));
		CurrentSketchPoints.RemoveAt(0, 100); // İlk 100 noktayı çıkar
	}
}

void SSketchWidget::ClearCurrentSketch()
{
	CurrentSketchPoints.Empty();
}

FVector2D SSketchWidget::ScreenSpaceToGraphSpace(const FVector2D& ScreenSpacePosition) const
{
	if (GraphPanel)
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(GraphPanel->AsShared());
		if (Window.IsValid())
		{
			FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
			FVector2D LocalPosition = PanelGeometry.AbsoluteToLocal(ScreenSpacePosition);
			return GraphPanel->PanelCoordToGraphCoord(LocalPosition);
		}
	}
	return FVector2D::ZeroVector;
} 
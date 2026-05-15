// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Application/IInputProcessor.h"
#include "SGraphPanel.h"
#include "SNexxNoteDrawingOverlay.h"
#include "Input/Events.h"
#include "Templates/SharedPointer.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"

/**
 * Blueprint Editor için input işleme yardımcısı.
 * Bu sınıf, tüm input olaylarını yakalar ve gerekirse işler.
 */
class FNexxNoteInputProcessor : public IInputProcessor
{
public:
	FNexxNoteInputProcessor(SGraphPanel* InGraphPanel, TSharedPtr<SNexxNoteDrawingOverlay> InOverlay)
		: GraphPanel(InGraphPanel)
		, DrawingOverlay(InOverlay)
		, bIsDrawing(false)
		, bWasInsidePanel(false)
	{}

	virtual ~FNexxNoteInputProcessor() 
	{
		// Yıkıcı metotta tüm imleç ve çizim durumlarını temizle
		ResetCursor();
		
		// Çizim modunu kapatalım
		bIsDrawing = false;
		
		// Platform imleç tipini Default'a sıfırla
		if (FSlateApplication::IsInitialized())
		{
			TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();
			if (PlatformCursor.IsValid())
			{
				PlatformCursor->SetType(EMouseCursor::Default);
			}
		}
	}
	
	// İmleci tamamen resetle ve tüm fare yakalamalarını bırak
	void CompleteReset()
	{
		// İmleç sıfırla
		ResetCursor();
		
		// Çizim modunu kapatalım
		bIsDrawing = false;
		
		// Fare yakalamaları bırakılsın
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().ReleaseAllPointerCapture();
			
			// Platform imlecini sıfırla
			TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();
			if (PlatformCursor.IsValid())
			{
				PlatformCursor->SetType(EMouseCursor::Default);
			}
		}
		
		// Olay işleyicilerini kaldır
		if (GraphPanel)
		{
			GraphPanel->SetOnMouseButtonDown(FPointerEventHandler());
			GraphPanel->SetOnMouseButtonUp(FPointerEventHandler());
			GraphPanel->SetOnMouseMove(FPointerEventHandler());
			GraphPanel->OnMouseLeave(FPointerEvent());
		}
		
		UE_LOG(LogTemp, Warning, TEXT("[NexxNoteInput] CompleteReset() çağrıldı, tüm çizim durumu sıfırlandı"));
	}
	
	// Sketch modunun aktif olup olmadığını dışarıdan sorgula/ayarla
	void SetSketchModeActive(bool bActive)
	{
		// Sketch modu kapatılıyorsa komple temizlik yap
		if (!bActive && bIsDrawing)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NexxNoteInput] Sketch modu kapatılıyor, tüm durumu sıfırlıyorum"));
			CompleteReset();
		}
		
		// Çizim modun durumunu ayarla
		bIsDrawing = bActive && bIsDrawing;
		
		// GraphPanel üzerindeki imleci de ayarla
		if (GraphPanel)
		{
			GraphPanel->SetCursor(bActive && bWasInsidePanel ? EMouseCursor::Crosshairs : EMouseCursor::Default);
		}
	}

	// IInputProcessor arayüzü
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override 
	{
		// Sketch modu aktif değilse bir şey yapmayalım
		if (!DrawingOverlay.IsValid() || !GraphPanel || !DrawingOverlay->IsSketchModeActive())
		{
			return;
		}
		
		// Mevcut fare konumunu al
		FVector2D MousePosition = SlateApp.GetCursorPos();
		
		// GraphPanel koordinatlarına dönüştür
		FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
		FVector2D LocalPos = PanelGeometry.AbsoluteToLocal(MousePosition);
		FVector2D PanelSize = PanelGeometry.GetLocalSize();
		
		// İçeride mi dışarıda mı kontrol et
		bool bIsInsidePanel = (LocalPos.X >= 0 && LocalPos.Y >= 0 && LocalPos.X <= PanelSize.X && LocalPos.Y <= PanelSize.Y);
		
		// Fare basılı olma durumunu kontrol et - FSlateApplication.IsMouseButtonDown yerine doğrudan durum kontrolü
		bool bIsMouseDown = SlateApp.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
		
		// ÖNEMLİ: Fare basılı ve çizim yapılıyorsa, panel içi/dışı farketmeksizin çizim modunu devam ettir
		if (bIsMouseDown && bIsDrawing)
		{
			// Çizim devam ediyor, burada fare imlecini değiştirmiyoruz
			// Bu sayede çizim yaparken panel dışına çıksak bile çizim modunda kalırız
			return;
		}
		
		// Fare basılı değilse veya çizim yapılmıyorsa, panel içi/dışı durumuna göre imleci değiştir
		if (bIsInsidePanel != bWasInsidePanel || (!bIsMouseDown && bIsDrawing))
		{
			// Fare düğmesi bırakıldıysa çizim modunu kapat
			if (!bIsMouseDown && bIsDrawing)
			{
				bIsDrawing = false;
				UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Tick detected: Mouse button released, ending drawing"));
			}
			
			if (bIsInsidePanel)
			{
				// Panel içine girdik
				UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Tick detected: Mouse ENTERED panel area"));
				GraphPanel->SetCursor(EMouseCursor::Crosshairs);
			}
			else
			{
				// Panel dışına çıktık
				UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Tick detected: Mouse LEFT panel area"));
				GraphPanel->SetCursor(EMouseCursor::Default);
				
				// İlave güvenlik: Platform imlecini de sıfırla
				TSharedPtr<ICursor> PlatformCursor = SlateApp.GetPlatformCursor();
				if (PlatformCursor.IsValid())
				{
					PlatformCursor->SetType(EMouseCursor::Default);
				}
			}
			
			// Durumu kaydet
			bWasInsidePanel = bIsInsidePanel;
		}
	}
	
	// İmleci varsayılan haline resetle
	void ResetCursor();
	
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		// Escape tuşuyla çizim modunu kapatabilir
		return false;
	}
	
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		return false;
	}
	
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (!DrawingOverlay.IsValid() || !GraphPanel || !DrawingOverlay->IsSketchModeActive())
		{
			return false;
		}
		
		// Mouse pozisyonunu kontrol et - GraphPanel içinde mi?
		FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
		FVector2D LocalPos = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		
		// Panel sınırları içinde mi?
		FVector2D PanelSize = PanelGeometry.GetLocalSize();
		if (LocalPos.X >= 0 && LocalPos.Y >= 0 && LocalPos.X <= PanelSize.X && LocalPos.Y <= PanelSize.Y)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NexxNoteInput] Mouse button down INSIDE panel! Pos: %s"), *LocalPos.ToString());
			
			// Sol tuşa tıklanırsa
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				bIsDrawing = true;
				DrawingOverlay->OnMouseButtonDown(PanelGeometry, MouseEvent);
				return true; // Olayı işledik
			}
			// Sağ tuşa tıklanırsa
			else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
			{
				bIsDrawing = false;
				DrawingOverlay->OnMouseButtonDown(PanelGeometry, MouseEvent);
				return true;
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Mouse button down OUTSIDE panel"));
		}
		
		return false;
	}
	
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		// Önce çizim modu aktif mi kontrol et
		if (!DrawingOverlay.IsValid() || !GraphPanel || !DrawingOverlay->IsSketchModeActive())
		{
			return false;
		}
		
		// Sadece sol fare tuşu ile ilgileniyoruz ve çizim yapıyorduk
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsDrawing)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NexxNoteInput] Mouse button up, finishing drawing"));
			
			// Panel içinde miyiz?
			FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
			FVector2D LocalPos = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			FVector2D PanelSize = PanelGeometry.GetLocalSize();
			bool bIsInsidePanel = (LocalPos.X >= 0 && LocalPos.Y >= 0 && LocalPos.X <= PanelSize.X && LocalPos.Y <= PanelSize.Y);
			
			// Panel içindeysek, overlay'e olayı ilet
			if (bIsInsidePanel)
			{
			DrawingOverlay->OnMouseButtonUp(PanelGeometry, MouseEvent);
				// İmleci çizim modunda tut
				GraphPanel->SetCursor(EMouseCursor::Crosshairs);
			}
			else
			{
				// Panel dışındaysak normal imleç göster
				GraphPanel->SetCursor(EMouseCursor::Default);
				
				// Platform imlecini de direkt değiştirelim
				TSharedPtr<ICursor> PlatformCursor = SlateApp.GetPlatformCursor();
				if (PlatformCursor.IsValid())
				{
					PlatformCursor->SetType(EMouseCursor::Default);
				}
				
				UE_LOG(LogTemp, Warning, TEXT("[NexxNoteInput] Mouse button up OUTSIDE panel, forcing normal cursor"));
			}
			
			// Durumu kaydet - artık panel içinde/dışında olduğumuzu biliyoruz
			bWasInsidePanel = bIsInsidePanel;
			
			// ÇIZIMI SONLANDIR - bu çok önemli!
			// Fare bırakıldığında çizim işlemi bitmiş olmalı - ister panel içinde, ister dışında olsun
			bIsDrawing = false;
			
			// Fare yakalamasını bırak - Her platform için çalışacak şekilde tasarlandı
			if (SlateApp.HasAnyMouseCaptor())
			{
				UE_LOG(LogTemp, Warning, TEXT("[NexxNoteInput] Detected mouse capture, will be released"));
				SlateApp.ReleaseAllPointerCapture();
				
				// Platform imlecini bir kez daha sıfırla
				TSharedPtr<ICursor> PlatformCursor = SlateApp.GetPlatformCursor();
				if (PlatformCursor.IsValid())
				{
					PlatformCursor->SetType(bIsInsidePanel ? EMouseCursor::Crosshairs : EMouseCursor::Default);
				}
			}
			
			// ÇOK ÖNEMLI: Input ayarlarını tamamen resetle
			SlateApp.ResetToDefaultPointerInputSettings();
			
			// Olayı işledik
			return true;
		}
		
		return false;
	}
	
	// Panel üzerine fare girdiğinde veya çıktığında imleç ayarı
	bool HandleMouseEnterEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		// Çizim modu aktifse ve overlay/panel geçerliyse
		if (DrawingOverlay.IsValid() && GraphPanel && DrawingOverlay->IsSketchModeActive())
		{
			// Panel üzerine geldiğimizde çizim imleci göster
			UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Mouse entered widget area, setting Crosshairs cursor"));
			GraphPanel->SetCursor(EMouseCursor::Crosshairs);
			return true;
		}
		return false;
	}
	
	bool HandleMouseLeaveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		// Panel alanından çıkınca normal imlece dön
		if (DrawingOverlay.IsValid() && GraphPanel)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Mouse left widget area, setting Default cursor"));
			GraphPanel->SetCursor(EMouseCursor::Default);
			
			// Platform imlecini de varsayılana döndür
			TSharedPtr<ICursor> PlatformCursor = SlateApp.GetPlatformCursor();
			if (PlatformCursor.IsValid())
			{
				PlatformCursor->SetType(EMouseCursor::Default);
			}
			
			return true;
		}
		return false;
	}
	
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (!DrawingOverlay.IsValid() || !GraphPanel || !DrawingOverlay->IsSketchModeActive())
		{
			return false;
		}
		
		// Mouse pozisyonunu kontrol et - GraphPanel içinde mi?
		FGeometry PanelGeometry = GraphPanel->GetTickSpaceGeometry();
		FVector2D LocalPos = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		
		// Panel sınırları içinde mi kontrolü ekle
		FVector2D PanelSize = PanelGeometry.GetLocalSize();
		bool bIsInsidePanel = (LocalPos.X >= 0 && LocalPos.Y >= 0 && LocalPos.X <= PanelSize.X && LocalPos.Y <= PanelSize.Y);
		
		// ÖNEMLİ: Çizim yapıyorsak (fare tuşu basılıyken) - özel durum
		if (bIsDrawing && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			// Çizim modundayken, sadece panel içindeyken gerçekten çiz
			if (bIsInsidePanel)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Mouse move while drawing INSIDE panel"));
		DrawingOverlay->OnMouseMove(PanelGeometry, MouseEvent);
			}
			
			// Fare yakalanmışsa, panel içinde olsak da olmasak da olayı işledik say
			// Bu sayede çizim sırasında fare panel dışına çıksa bile fare yakalama devam eder
			if (SlateApp.HasAnyMouseCaptor())
			{
				return true;
			}
		}
		
		// Normal çizim modu davranışı (fare basılı değilken)
		// İmleci panel içi/dışına göre değiştir
		if (bIsInsidePanel)
		{
			// Panel içindeyse ve çizim modu aktifse - çizim imleci (varsa)
			GraphPanel->SetCursor(EMouseCursor::Crosshairs);
			
			UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Mouse INSIDE panel, setting cursor to Crosshairs"));
			return true; // Panel içindeyken olayı işledik
		}
		else
		{
			// Panel dışındaysa normal imleç
			GraphPanel->SetCursor(EMouseCursor::Default);
			
			// Platform imlecini de direkt değiştirelim
			TSharedPtr<ICursor> PlatformCursor = SlateApp.GetPlatformCursor();
			if (PlatformCursor.IsValid())
			{
				PlatformCursor->SetType(EMouseCursor::Default);
			}
			
			UE_LOG(LogTemp, Verbose, TEXT("[NexxNoteInput] Mouse OUTSIDE panel, setting cursor to Default"));
			
			// Çizim yapıyorduk ama panel dışına çıktık
			if (bIsDrawing)
			{
				UE_LOG(LogTemp, Warning, TEXT("[NexxNoteInput] Mouse moved OUTSIDE panel while drawing"));
				
				// Fare yakalanmışsa, sadece panel dışına çıktık diye çizimi sonlandırmayalım
				// Ama imleci normal hale getirelim
				if (SlateApp.HasAnyMouseCaptor())
				{
					// Burada sadece olayı işlediğimizi söylüyoruz, çizim olmayacak
		return true;
				}
			}
		}
		
		// Panel dışında olduğumuzda ve çizim yapmıyorsak olayı işlemiyoruz
		// Böylece normal UI olaylarının çalışmasına izin veriyoruz
		return bIsInsidePanel;
	}
	
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		return false;
	}

private:
	/** GraphPanel referansı */
	SGraphPanel* GraphPanel;
	
	/** Overlay referansı */
	TSharedPtr<SNexxNoteDrawingOverlay> DrawingOverlay;
	
	/** Çizim yapılıyor mu? */
	bool bIsDrawing;
	
	/** Önceki durum */
	bool bWasInsidePanel = false;
};

/**
 * Blueprint Editor için Graph Panel genişletmesi.
 * Bu sınıf, GraphPanel'e çizim ve not ekleme özelliklerini entegre eder.
 */
class NEXXNOTE_API FNexxNoteGraphPanelExtension
{
public:
	/** Constructor */
	FNexxNoteGraphPanelExtension();

	/** Destructor */
	~FNexxNoteGraphPanelExtension();
	
	/** Extension aktif mi? */
	bool IsExtensionActive() const;
	
	/**
	 * Özelliği kayıt eder
	 */
	void Register();
	
	/**
	 * Özelliğin kaydını kaldırır
	 */
	void Unregister();
	
	/**
	 * Sketch modunu aktif eder
	 */
	void SetSketchModeActive(bool bActive);
	
	/**
	 * Not modunu aktif eder
	 */
	void SetNoteModeActive(bool bActive);
	
	/**
	 * Sketch modu devre dışı bırakıldığında çağrılır
	 */
	void OnSketchModeDeactivated();
	
	/**
	 * Blueprint Editor'leri kontrol et
	 */
	void CheckForBlueprintEditors();
	
	/**
	 * Tüm overlay'leri temizle
	 */
	void ClearAllOverlays();
	
	/** Sketch modu durumu değiştiğinde çağrılan delegate */
	DECLARE_DELEGATE_OneParam(FOnSketchModeStateChanged, bool /** bActive */);
	FOnSketchModeStateChanged OnSketchModeStateChanged;
	
	/** Not modu durumu değiştiğinde çağrılan delegate */
	DECLARE_DELEGATE_OneParam(FOnNoteModeStateChanged, bool /** bActive */);
	FOnNoteModeStateChanged OnNoteModeStateChanged;
	
	/** Çizim rengini ayarla */
	void SetDrawingColor(const FLinearColor& NewColor);
	/** Çizim kalınlığını ayarla */
	void SetDrawingThickness(float NewThickness);
	FLinearColor GetCurrentDrawingColor() const;
	float GetCurrentDrawingThickness() const;
	
private:
	/** Engine başlatıldıktan sonra çağrılır */
	void OnPostEngineInit();
	
	/** Her framede çağrılır */
	bool Tick(float DeltaTime);
	
	/** Yeni bir GraphPanel bulunduğunda çağrılır */
	void HandleNewGraphPanel(SGraphPanel* GraphPanel);
	
	/** Mevcut görünüm pozisyonunda bir not oluştur */
	void CreateNoteAtCurrentPosition();
	
	/** Slate Tick delegesi */
	FTSTicker::FDelegateHandle TickDelegate;

	/** SGraphPanel - Sketch widget haritası */
	TMap<SGraphPanel*, TSharedPtr<SNexxNoteDrawingOverlay>> GraphPanelToSketchWidgetMap;
	
	/** SGraphPanel - Overlay widget haritası */
	TMap<SGraphPanel*, TSharedPtr<SNexxNoteDrawingOverlay>> GraphPanelToOverlayMap;
	
	/** Sketch modu aktif mi */
	bool bSketchModeActive = false;
	
	/** Not modu aktif mi */
	bool bNoteModeActive = false;
	
	/** Çizim rengi - modül seviyesinde saklanan değer */
	FLinearColor SavedDrawingColor = FLinearColor::Yellow;
	
	/** Çizim kalınlığı - modül seviyesinde saklanan değer */
	float SavedDrawingThickness = 3.0f;
}; 
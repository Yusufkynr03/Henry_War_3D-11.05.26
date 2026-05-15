// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "SNexxNoteColorPicker.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Modules/ModuleManager.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "NexxNoteColorPicker"

// Add a log category for debugging
DEFINE_LOG_CATEGORY_STATIC(LogNexxNoteColorPicker, Log, All);

// Define icon paths - relative to plugin Resources directory
static const FString SketchIconPath = FString(TEXT("Resources/Sketchicon.png"));
static const FString ColorChangerIconPath = FString(TEXT("Resources/Colorchangericon.png"));
static const FString NoteIconPath = FString(TEXT("Resources/Noteicon.png"));

// MANUEL AYARLAR: Renk önizleme kutusunun boyutu
// NOT: Boyutu değiştirirseniz, köşe yarıçapı da otomatik olarak yarısı olur (tam oval için)
static const float PreviewBoxSize = 20.0f; // MANUEL AYAR: İstediğiniz boyut (daha küçük)
static const float PreviewCornerRadius = PreviewBoxSize / 2.0f; // Tam daire/oval için

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SNexxNoteColorPicker::Construct(const FArguments& InArgs)
{
	SelectedColor = InArgs._InitialColor.Get();
	SelectedThickness = InArgs._InitialThickness.Get();
	OnColorSelected = InArgs._OnColorSelected;
	OnThicknessSelected = InArgs._OnThicknessSelected;
	
	// Load icon brushes from plugin Resources directory
	FString PluginBaseDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("NexxNote"));
	
	// Create brushes for the icons
	SketchIconBrush = MakeShareable(new FSlateImageBrush(
		FPaths::Combine(PluginBaseDir, SketchIconPath), FVector2D(24, 24)));
	
	ColorChangerIconBrush = MakeShareable(new FSlateImageBrush(
		FPaths::Combine(PluginBaseDir, ColorChangerIconPath), FVector2D(24, 24)));
	
	NoteIconBrush = MakeShareable(new FSlateImageBrush(
		FPaths::Combine(PluginBaseDir, NoteIconPath), FVector2D(24, 24)));
	
	// Kalınlık değerini gösteren metin için TAttribute oluştur
	ThicknessTextAttr = TAttribute<FText>::Create([this]() { 
		return FText::AsNumber(SelectedThickness); 
	});
	
	// Önizleme kutusu için oval brush oluştur - sadece bir kez oluşturulur
	PreviewColorBrush = MakeShared<FSlateRoundedBoxBrush>(
		SelectedColor, // Renk
		PreviewCornerRadius, // MANUEL AYAR: Köşe yarıçapı (tam daire için kutunun yarısı)
		SelectedColor, // Dolgu rengi
		0.0f           // Kenarlık yok
	);
	
	// Basit renk paleti oluştur
	PresetColors = {
		FLinearColor::Red,
		FLinearColor::Green,
		FLinearColor::Blue,
		FLinearColor::Yellow,
		FLinearColor::Black,
		FLinearColor::White,
		FLinearColor(1.0f, 0.0f, 1.0f), // Magenta (el ile tanımlama)
		FLinearColor(0.0f, 1.0f, 1.0f)  // Cyan (el ile tanımlama)
	};
	
	// Her renk için yuvarlak köşeli brush'lar oluştur
	PresetBrushes.Empty();
	PresetBorderBrushes.Empty();
	SelectionGlowBrushes.Empty();

	// Highlight overlay'ler için yuvarlak köşeli brush'lar
	HighlightBrush = MakeShared<FSlateRoundedBoxBrush>(
		FLinearColor(1.0f, 1.0f, 1.0f, 0.10f), // Yarı şeffaf beyaz
		10.0f,                                 // Aynı köşe yarıçapı
		FLinearColor(1.0f, 1.0f, 1.0f, 0.10f), // Dolgu rengi
		0.0f                                   // Kenarlık yok
	);

	SelectionHighlightBrush = MakeShared<FSlateRoundedBoxBrush>(
		FLinearColor(1.0f, 1.0f, 1.0f, 0.05f), // Çok hafif beyaz
		10.0f,                                 // Aynı köşe yarıçapı
		FLinearColor(1.0f, 1.0f, 1.0f, 0.05f), // Dolgu rengi
		0.0f                                   // Kenarlık yok
	);

	// İç gölge efekti için brush
	InnerShadowBrush = MakeShared<FSlateRoundedBoxBrush>(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.2f), // Biraz daha belirgin siyah
		9.0f,                                 // Ana renkle hemen hemen aynı yarıçap (kenarlardaki etkiyi artırmak için)
		FLinearColor(0, 0, 0, 1),             // Şeffaf dolgu (sadece kenar olarak görünmesi için)
		0.5f                                  // Daha ince kenarlık (sadece kenarlar)
	);

	for (const FLinearColor& Color : PresetColors)
	{
		// Ana renk için brush - iç dolgu
		TSharedPtr<FSlateRoundedBoxBrush> RoundedBrush = MakeShared<FSlateRoundedBoxBrush>(
			Color,         // Taban rengi
			10.0f,          // Köşe yuvarlaklık yarıçapı (daha oval)
			Color,         // Dolgu rengi
			0.0f           // Kenarlık olmasın (dolgu için)
		);
		PresetBrushes.Add(RoundedBrush);
		
		// Kenarlık için ayrı brush
		TSharedPtr<FSlateRoundedBoxBrush> BorderBrush = MakeShared<FSlateRoundedBoxBrush>(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.3f),  // Hafif opak siyah
			9.0f,                                   // Köşe yarıçapı (daha oval)
			FLinearColor(0, 0, 0, 0),               // Şeffaf dolgu
			1.0f                                    // 1px kenarlık
		);
		PresetBorderBrushes.Add(BorderBrush);
		
		// Seçim glow efekti için parlayan brush (mavi ışıltı)
		TSharedPtr<FSlateRoundedBoxBrush> GlowBrush = MakeShared<FSlateRoundedBoxBrush>(
			FLinearColor(0.1f, 0.6f, 1.0f, 0.7f),   // Parlak mavi ışıltı
			10.0f,                                   // En dıştaki kenarlık, daha yuvarlak
			FLinearColor(0, 0, 0, 0),                // Şeffaf dolgu
			2.0f                                     // Daha kalın kenarlık
		);
		SelectionGlowBrushes.Add(GlowBrush);
	}
	
	ChildSlot
	[
		SNew(SMenuAnchor)
		.UseApplicationMenuStack(true)
		.OnGetMenuContent(this, &SNexxNoteColorPicker::GetMenuContent)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
		[
			SNew(SVerticalBox)
				// Modern üst bar - now using our icons
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
					.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
					.Padding(6)
					[
						SNew(SHorizontalBox)
						// Mode değiştirici buton - now with custom icon
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0, 0, 8, 0)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &SNexxNoteColorPicker::OnOpenFullColorPickerClicked)
							.ToolTipText(LOCTEXT("ColorPickerToolTip", "Open Color Picker tool"))
							.ContentPadding(0)
							.OnHovered_Lambda([this]() { bIsColorButtonHovered = true; Invalidate(EInvalidateWidgetReason::Paint); })
							.OnUnhovered_Lambda([this]() { bIsColorButtonHovered = false; Invalidate(EInvalidateWidgetReason::Paint); })
							[
								SNew(SBox)
								.WidthOverride(36.0f)
								.HeightOverride(36.0f)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										SNew(SBorder)
										.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
										.BorderBackgroundColor_Lambda([this]() -> FSlateColor {
											return bIsColorButtonHovered
												? FLinearColor(0.1f, 0.5f, 1.0f, 0.25f)
												: FLinearColor(0, 0, 0, 0);
										})
										.Padding(0)
									]
									+SOverlay::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.WidthOverride(20.0f)
										.HeightOverride(20.0f)
										[
											SNew(SImage)
											.Image(FAppStyle::Get().GetBrush("ColorPicker.Mode"))
											.ColorAndOpacity(FLinearColor::White)
										]
									]
								]
							]
						]
						// Renk preview
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						.Padding(0, 0, 10, 0)
						.FillWidth(1.0f)
						[
							SNew(SBox)
							// Preview box size
							.WidthOverride(PreviewBoxSize)
							.HeightOverride(PreviewBoxSize)
							[
								// Use SImage for clean oval
								SNew(SImage)
								.Image(PreviewColorBrush.Get())
							]
						]
				]
			]
			
			// Renk seçenekleri grid'i
			+SVerticalBox::Slot()
			.AutoHeight()
				.Padding(0, 0, 0, 15) // Alt padding: Renk grid'i ile kalınlık arasındaki boşluk
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
					.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
					.Padding(6)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FMargin(3.0f))
				
				// İlk sıra renkleri
						+SUniformGridPanel::Slot(0, 0) [MakeColorButton(PresetColors[0])]
						+SUniformGridPanel::Slot(1, 0) [MakeColorButton(PresetColors[1])]
						+SUniformGridPanel::Slot(2, 0) [MakeColorButton(PresetColors[2])]
						+SUniformGridPanel::Slot(3, 0) [MakeColorButton(PresetColors[3])]
				
				// İkinci sıra renkleri
						+SUniformGridPanel::Slot(0, 1) [MakeColorButton(PresetColors[4])]
						+SUniformGridPanel::Slot(1, 1) [MakeColorButton(PresetColors[5])]
						+SUniformGridPanel::Slot(2, 1) [MakeColorButton(PresetColors[6])]
						+SUniformGridPanel::Slot(3, 1) [MakeColorButton(PresetColors[7])]
				]
			]
			
			// Kalınlık slider'ı
			+SVerticalBox::Slot()
			.AutoHeight()
				// MANUEL AYAR: Kalınlık slider'ını yukarı kaydırmak için buradaki padding değerlerini değiştirin
				// Format: (Sol, Üst, Sağ, Alt) - Üst değeri azaltırsanız yukarı kayar
				.Padding(0, -1, 0, 0) // MANUEL AYAR: Negatif üst değer yukarı kaydırır
			[
				SNew(SVerticalBox)
				
				+SVerticalBox::Slot()
				.AutoHeight()
					// MANUEL AYAR: Kalınlık yazısı ile slider arasındaki boşluk
					// Yazıyı yukarı/aşağı taşımak için bu değeri ayarlayın
					.Padding(0, -10, 0, 5) // MANUEL AYAR: Negatif değer yukarı, pozitif değer aşağı kaydırır
				[
					SNew(SHorizontalBox)
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Thickness: ")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
					]
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(ThicknessTextAttr)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
					]
				]
				
				+SVerticalBox::Slot()
				.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
						.Padding(3, 5)
						[
							SNew(SBox)
							.HeightOverride(24.0f)
							.Padding(FMargin(1.0f, 2.0f))
				[
					SNew(SSlider)
								.Style(FAppStyle::Get(), "Slider")
					.Value(SelectedThickness)
					.MinValue(1.0f)
					.MaxValue(20.0f)
					.StepSize(1.0f)
					.OnValueChanged(this, &SNexxNoteColorPicker::OnThicknessChanged)
								.SliderBarColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
								.SliderHandleColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
								.Orientation(Orient_Horizontal)
							]
						]
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SNexxNoteColorPicker::MakeColorButton(const FLinearColor& Color)
{
	// Rengin PresetColors'daki indeksini bul
	int32 ColorIndex = PresetColors.Find(Color);
	check(ColorIndex != INDEX_NONE); // Debug için kontrol
	
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.OnClicked(FOnClicked::CreateLambda([this, Color]() -> FReply {
			return OnColorButtonClicked(Color);
		}))
		.ToolTipText(FText::FromString(TEXT("Select Color")))
		.ContentPadding(FMargin(2.0f))
		[
			SNew(SBox)
			.WidthOverride(30.0f)
			.HeightOverride(30.0f)
			[
				SNew(SOverlay)
				
				// Seçim glow efekti - dinamik görünürlük
				+SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(SelectionGlowBrushes[ColorIndex].Get())
					.Visibility_Lambda([this, Color]() {
						return SelectedColor.Equals(Color) ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Padding(1.0f)
				]
				
				// Normal kenarlık
				+SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(PresetBorderBrushes[ColorIndex].Get())
					.Padding(1.0f)
					[
						SNew(SBorder)
						.BorderImage(PresetBrushes[ColorIndex].Get())
						.Padding(0)
						// İç içe ışık yansıma efekti için bir SOverlay ekle
						[
							SNew(SOverlay)
							
							// İç gölge efekti
							+SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							.Padding(0)
							[
								SNew(SBorder)
								.BorderImage(InnerShadowBrush.Get())
								.Padding(0)
								.Visibility(EVisibility::HitTestInvisible)
							]
							
							// Parlak yüzey efekti - Üst yarım daire gradient
							+SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Top)
							.Padding(0, 0, 0, 0)
							[
								SNew(SBox)
								.HeightOverride(12.0f) // Yüksekliğin yarısı kadar
								[
									SNew(SBorder)
									.BorderImage(HighlightBrush.Get())
									.Padding(0)
									.Visibility(EVisibility::HitTestInvisible) // Input almayacak şekilde
								]
							]
							
							// Ekstra parlaklık efekti - seçili buton için
							+SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(SelectionHighlightBrush.Get())
								.Padding(0)
								.Visibility_Lambda([this, Color]() {
									return SelectedColor.Equals(Color) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
								})
							]
						]
					]
				]
				
				// Tik işareti - dinamik görünürlük
				+SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(0, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("✓")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(FLinearColor::White)
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
					.Visibility_Lambda([this, Color]() {
						return SelectedColor.Equals(Color) ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
			]
		];
}

FReply SNexxNoteColorPicker::OnColorButtonClicked(const FLinearColor& Color)
{
	SelectedColor = Color;
	UpdatePreviewColorBrush(); // Brush'ı güncelle
	if (OnColorSelected.IsBound())
	{
		OnColorSelected.Execute(SelectedColor);
	}
	
	// Daha güçlü yenileme fonksiyonu çağır
	ForceRefresh();
	
	return FReply::Handled();
}

void SNexxNoteColorPicker::OnThicknessChanged(float NewValue)
{
	SelectedThickness = NewValue;
	
	// Kalınlık değiştiyse delegatı çağır
	if (OnThicknessSelected.IsBound())
	{
		OnThicknessSelected.Execute(SelectedThickness);
	}
	
	// Yeniden çizim için widget'ı geçersiz kıl
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SNexxNoteColorPicker::OnColorBlockClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Sol tuşa tıklandıysa renk seçici penceresini aç
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OpenColorPicker();
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FReply SNexxNoteColorPicker::OnOpenFullColorPickerClicked()
{
	OpenColorPicker();
	return FReply::Handled();
}

void SNexxNoteColorPicker::OpenColorPicker()
{
	FColorPickerArgs PickerArgs;
	PickerArgs.bUseAlpha = false;
	PickerArgs.InitialColor = SelectedColor;
	// Canlı güncelleme ve OK için
	PickerArgs.bOnlyRefreshOnMouseUp = false;
	PickerArgs.bOnlyRefreshOnOk = false;
	// Sadece OnColorCommitted kullan
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SNexxNoteColorPicker::HandleColorCommitted);
	::OpenColorPicker(PickerArgs);
}

// Handler for committed color changes
void SNexxNoteColorPicker::HandleColorCommitted(FLinearColor NewColor)
{
	SelectedColor = NewColor;
	UpdatePreviewColorBrush(); // Brush'ı güncelle
	Invalidate(EInvalidateWidgetReason::Paint);
	OnColorSelected.ExecuteIfBound(SelectedColor);
}

// Tüm widget'ı tamamen yeniden çiz ve yeniden düzenle
void SNexxNoteColorPicker::ForceRefresh()
{
	// Tüm widget'ı ve alt widget'larını yeniden çiz
	Invalidate(EInvalidateWidgetReason::Prepass);
	
	// Alt widget'ları yenile
	if (GetParentWidget().IsValid())
	{
		// Use FChildren to properly access child widgets
		FChildren* Children = GetParentWidget()->GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedRef<SWidget> Child = Children->GetChildAt(i);
			Child->Invalidate(EInvalidateWidgetReason::Layout);
			Child->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
	
	// Kendimizdeki cached layout'ı sıfırla
	Invalidate(EInvalidateWidgetReason::Layout);
	Invalidate(EInvalidateWidgetReason::Paint);
	
	// FSlateApplication seviyesinde yenileme
	if (FSlateApplication::IsInitialized())
	{
		// Bu widget'ı görünen bir widget path'ı içinde ara ve yeniden çiz
		if (FSlateApplication::Get().GetActiveTopLevelWindow().IsValid())
		{
			TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
			ActiveWindow->Invalidate(EInvalidateWidgetReason::ChildOrder);
			ActiveWindow->Invalidate(EInvalidateWidgetReason::Prepass);
		}
	}
}

// Renk değiştiğinde önizleme kutusu brush'ını güncelle
void SNexxNoteColorPicker::UpdatePreviewColorBrush()
{
	// Brush'ı tamamen yenile - daima oval kalması için
	// FSlateRoundedBoxBrush içinde TintColor ile güncelleme yapmak yerine
	// Bütün bir brush yeniden oluşturulur
	*(PreviewColorBrush) = FSlateRoundedBoxBrush(
		SelectedColor, // Renk
		PreviewCornerRadius, // MANUEL AYAR: Köşe yarıçapı (her zaman oval için kutunun yarısı)
		SelectedColor, // Dolgu rengi
		0.0f           // Kenarlık yok
	);

	// Widget'ı yeniden çizim için işaretle
	Invalidate(EInvalidateWidgetReason::Paint);
}

TSharedRef<SWidget> SNexxNoteColorPicker::GetMenuContent()
{
	return SNew(SVerticalBox)
		// Use our existing content
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				// Modern üst bar - now using our icons
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 10)
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
					.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
					.Padding(6)
					[
						SNew(SHorizontalBox)
						// Mode değiştirici buton - now with custom icon
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0, 0, 8, 0)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &SNexxNoteColorPicker::OnOpenFullColorPickerClicked)
							.ToolTipText(LOCTEXT("ColorPickerToolTip", "Open Color Picker tool"))
							.ContentPadding(0)
							.OnHovered_Lambda([this]() { bIsColorButtonHovered = true; Invalidate(EInvalidateWidgetReason::Paint); })
							.OnUnhovered_Lambda([this]() { bIsColorButtonHovered = false; Invalidate(EInvalidateWidgetReason::Paint); })
							[
								SNew(SBox)
								.WidthOverride(36.0f)
								.HeightOverride(36.0f)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										SNew(SBorder)
										.BorderImage(FAppStyle::Get().GetBrush("Button.Normal"))
										.BorderBackgroundColor_Lambda([this]() -> FSlateColor {
											return bIsColorButtonHovered
												? FLinearColor(0.1f, 0.5f, 1.0f, 0.25f)
												: FLinearColor(0, 0, 0, 0);
										})
										.Padding(0)
									]
									+SOverlay::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.WidthOverride(20.0f)
										.HeightOverride(20.0f)
										[
											SNew(SImage)
											.Image(FAppStyle::Get().GetBrush("ColorPicker.Mode"))
											.ColorAndOpacity(FLinearColor::White)
										]
									]
								]
							]
						]
						// Renk preview
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						.Padding(0, 0, 10, 0)
						.FillWidth(1.0f)
						[
							SNew(SBox)
							// Preview box size
							.WidthOverride(PreviewBoxSize)
							.HeightOverride(PreviewBoxSize)
							[
								// Use SImage for clean oval
								SNew(SImage)
								.Image(PreviewColorBrush.Get())
							]
						]
					]
				]
				
				// Renk seçenekleri grid'i
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 15) // Alt padding: Renk grid'i ile kalınlık arasındaki boşluk
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
					.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
					.Padding(6)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FMargin(3.0f))
						
						// İlk sıra renkleri
						+SUniformGridPanel::Slot(0, 0) [MakeColorButton(PresetColors[0])]
						+SUniformGridPanel::Slot(1, 0) [MakeColorButton(PresetColors[1])]
						+SUniformGridPanel::Slot(2, 0) [MakeColorButton(PresetColors[2])]
						+SUniformGridPanel::Slot(3, 0) [MakeColorButton(PresetColors[3])]
						
						// İkinci sıra renkleri
						+SUniformGridPanel::Slot(0, 1) [MakeColorButton(PresetColors[4])]
						+SUniformGridPanel::Slot(1, 1) [MakeColorButton(PresetColors[5])]
						+SUniformGridPanel::Slot(2, 1) [MakeColorButton(PresetColors[6])]
						+SUniformGridPanel::Slot(3, 1) [MakeColorButton(PresetColors[7])]
					]
				]
				
				// Kalınlık slider'ı
				+SVerticalBox::Slot()
				.AutoHeight()
				// MANUEL AYAR: Kalınlık slider'ını yukarı kaydırmak için buradaki padding değerlerini değiştirin
				// Format: (Sol, Üst, Sağ, Alt) - Üst değeri azaltırsanız yukarı kayar
				.Padding(0, -1, 0, 0) // MANUEL AYAR: Negatif üst değer yukarı kaydırır
				[
					SNew(SVerticalBox)
					
					+SVerticalBox::Slot()
					.AutoHeight()
					// MANUEL AYAR: Kalınlık yazısı ile slider arasındaki boşluk
					// Yazıyı yukarı/aşağı taşımak için bu değeri ayarlayın
					.Padding(0, -10, 0, 5) // MANUEL AYAR: Negatif değer yukarı, pozitif değer aşağı kaydırır
					[
						SNew(SHorizontalBox)
						
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Thickness: ")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
						]
						
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(ThicknessTextAttr)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
						]
					]
					
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
						.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
						.Padding(3, 5)
						[
							SNew(SBox)
							.HeightOverride(24.0f)
							.Padding(FMargin(1.0f, 2.0f))
							[
								SNew(SSlider)
								.Style(FAppStyle::Get(), "Slider")
								.Value(SelectedThickness)
								.MinValue(1.0f)
								.MaxValue(20.0f)
								.StepSize(1.0f)
								.OnValueChanged(this, &SNexxNoteColorPicker::OnThicknessChanged)
								.SliderBarColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
								.SliderHandleColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
								.Orientation(Orient_Horizontal)
							]
						]
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
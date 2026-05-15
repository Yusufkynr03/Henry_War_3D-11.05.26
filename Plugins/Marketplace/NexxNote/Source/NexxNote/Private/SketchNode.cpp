// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "SketchNode.h"
#include "K2Node.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Styling/AppStyle.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Internationalization/Text.h"
#include "EdGraphSchema_K2_Actions.h"
#include "UObject/Class.h"
#include "Containers/Array.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "EdGraph/EdGraphPin.h"

#define LOCTEXT_NAMESPACE "SketchNode"

USketchNode::USketchNode()
{
	// Varsayılan değerler
	DrawColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); // Kırmızı
	DrawThickness = 2.0f;
	
	// Benzersiz tanımlayıcı oluştur
	SketchGuid = FGuid::NewGuid();
	
	// NodeGuid'i başlat - eksik olan ve hataya neden olan kısım
	NodeGuid = FGuid::NewGuid();
	
	// Node'un görünüş ayarları
	bCanRenameNode = false;
	bCanResizeNode = false;

	// Çakışma olasılığını azaltmak için title'ı görünmez yapıyoruz
	NodeWidth = 10;
	NodeHeight = 10;
    
    // Documentation node ayarları 
    // (UEdGraphNode_Documentation sınıfı için özel ayarlar)
    
    // Linkleri, URL'yi ve açıklamayı boş bırak
    Link = FString();
    Excerpt = FString();
}

void USketchNode::PostLoad()
{
    // Üst sınıfın PostLoad metodunu çağır
    Super::PostLoad();
    
    // Bu node'un derleme sürecine katılmadığından emin olmak için özel bayraklar ayarla
    // UEdGraphNode_Documentation'a özgü ayarlar 
    
    // Boş dokümantasyon metinleri
    this->Link = FString();
    this->Excerpt = FString();
}

void USketchNode::AllocateDefaultPins()
{
	// Bu node'un pin'leri olmasına gerek yok
	// Boş bırakıyoruz
}

FText USketchNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(TEXT("Sketch"));
}

FLinearColor USketchNode::GetNodeTitleColor() const
{
	return DrawColor;
}

FText USketchNode::GetTooltipText() const
{
	return LOCTEXT("SketchNodeTooltip", "A sketch drawn on the Blueprint Event Graph");
}

FSlateIcon USketchNode::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = DrawColor;
	static FSlateIcon Icon("EditorStyle", "Graph.Note.Small");
	return Icon;
}

bool USketchNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* InSchema) const
{
	// Sadece Blueprint graflarında kullanılabilir
	return InSchema->IsA<UEdGraphSchema_K2>();
}

void USketchNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// This node shouldn't appear in the context menu
}

FText USketchNode::GetMenuCategory() const
{
	return LOCTEXT("SketchNodeCategory", "Nexx|Sketches");
}

void USketchNode::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // Documentation node olduğumuzdan, derleme sırasında herhangi bir işlem yapmamıza gerek yok
    // KismetCompiler bu tür nodeları otomatik olarak atlar
    
    // Basit bir log mesajı bırakıyoruz
    CompilerContext.MessageLog.Note(TEXT("@@ is a sketch node and will be skipped during compilation"), this);
}

bool USketchNode::IsNodePure() const
{
    return false;
}

bool USketchNode::IsNodeComment() const
{
    return true;
}

bool USketchNode::ShouldOverridePinNames() const
{
    return false;
}

void USketchNode::SetDrawingPoints(const TArray<FVector2D>& InPoints)
{
	// Yeni çizim noktalarını kaydet
	DrawPoints = InPoints;
	
	// Segment bilgisini temizle (tüm noktalar tek segment)
	StrokeBreakIndices.Empty();

	// Blueprint editörünü güncelle
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void USketchNode::SetSketchData(const TArray<FVector2D>& InPoints, const FLinearColor& InColor, float InThickness)
{
	// Çizim noktalarını, rengini ve kalınlığını ayarla
	DrawPoints = InPoints;
	DrawColor = InColor;
	DrawThickness = InThickness;
	
	// Segment bilgisini temizle (tüm noktalar tek segment)
	StrokeBreakIndices.Empty();

	// Blueprint editörünü güncelle
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void USketchNode::SetSketchData(const TArray<FVector2D>& InPoints, const TArray<int32>& InBreakIndices, const FLinearColor& InColor, float InThickness)
{
	// Çizim noktalarını, rengini ve kalınlığını ayarla
	DrawPoints = InPoints;
	StrokeBreakIndices = InBreakIndices;
	DrawColor = InColor;
	DrawThickness = InThickness;

	// Blueprint editörünü güncelle
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void USketchNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	// Otomatik bağlantı yapmıyoruz
}

#undef LOCTEXT_NAMESPACE 
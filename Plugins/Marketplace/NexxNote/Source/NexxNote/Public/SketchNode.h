// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "UObject/ObjectMacros.h"
#include "SketchNode.generated.h"

/**
 * Blueprint Event Graph üzerinde çizim yapılabilmesini sağlayan Node.
 * Kullanıcının çizdiği noktaları saklar ve gösterir.
 */
UCLASS(MinimalAPI)
class USketchNode : public UEdGraphNode_Documentation
{
	GENERATED_BODY()

public:
	USketchNode();
	
	// Düğümü derleme sürecinden tamamen çıkarmak için
	virtual void PostLoad() override;
	virtual bool IsNodeGhost() const { return true; }
	
	// UEdGraphNode_Documentation interface
	// Burada UEdGraphNode_Documentation'a özgü metodları override edebiliriz
	// End of UEdGraphNode_Documentation interface

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutWidth, int32& OutHeight) const { return true; }
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* InSchema) const override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	// End of UEdGraphNode interface
	
	// "unsure" hatalarını önlemek için gerekli metodlar
	virtual bool IsNodePure() const;
	virtual bool IsNodeComment() const;
	virtual bool ShouldOverridePinNames() const;

	// Tooltip metodu ekleyelim
	virtual FText GetTooltipText() const;

	// Menu metotları ekleyelim
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const;
	virtual FText GetMenuCategory() const;

	// Expand node metodu ekleyelim - KismetCompiler için önemli
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);

	// Çizim noktalarını kaydet
	UPROPERTY()
	TArray<FVector2D> DrawPoints;
	
	// Farklı segment'lerin başlangıç indekslerini tutan dizi
	UPROPERTY()
	TArray<int32> StrokeBreakIndices;

	// Çizim rengi
	UPROPERTY(EditAnywhere, Category = "Sketch")
	FLinearColor DrawColor;

	// Çizgi kalınlığı
	UPROPERTY(EditAnywhere, Category = "Sketch")
	float DrawThickness;
	
	// Benzersiz tanımlayıcı
	UPROPERTY()
	FGuid SketchGuid;

	// Yeni çizim noktalarını ekle
	void SetDrawingPoints(const TArray<FVector2D>& InPoints);

	// Tüm çizim verilerini ayarla (noktalar, renk, kalınlık)
	void SetSketchData(const TArray<FVector2D>& InPoints, const FLinearColor& InColor, float InThickness);
	
	// Tüm çizim verilerini segment bilgisi ile birlikte ayarla
	void SetSketchData(const TArray<FVector2D>& InPoints, const TArray<int32>& InBreakIndices, const FLinearColor& InColor, float InThickness);

	// TArray<FVector2D> formatında çizim noktalarını döndür
	const TArray<FVector2D>& GetDrawingPoints() const { return DrawPoints; }
	
	// Segment başlangıç indekslerini döndür
	const TArray<int32>& GetStrokeBreakIndices() const { return StrokeBreakIndices; }
}; 
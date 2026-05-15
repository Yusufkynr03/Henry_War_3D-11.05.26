// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "UObject/ObjectMacros.h"
#include "UNexxNoteNode.generated.h"

/**
 * Blueprint Event Graph üzerinde not eklenmesini sağlayan Node.
 * Kullanıcının eklediği not içeriğini saklar ve gösterir.
 */
UCLASS(MinimalAPI)
class UNexxNoteNode : public UEdGraphNode_Documentation
{
	GENERATED_BODY()

public:
	// FObjectInitializer ile constructor tanımlıyoruz - Unreal Engine'in beklediği şekilde
	UNexxNoteNode(const FObjectInitializer& ObjectInitializer);
	
	// Düğümü derleme sürecinden tamamen çıkarmak için
	virtual void PostLoad() override;
	virtual bool IsNodeGhost() const { return true; }

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End of UEdGraphNode interface
	
	// "unsure" hatalarını önlemek için gerekli metodlar
	virtual bool IsNodePure() const;
	virtual bool IsNodeComment() const;
	virtual bool ShouldOverridePinNames() const;
	
	// Expand node metodu ekleyelim - KismetCompiler için önemli
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);

	// Not metni
	UPROPERTY(EditAnywhere, Category = "Note")
	FText NoteText;

	// Not boyutu
	UPROPERTY(EditAnywhere, Category = "Note")
	FVector2D NoteSize;
	
	// Font boyutu
	UPROPERTY(EditAnywhere, Category = "Note")
	int32 FontSize;
	
	// Not rengi
	UPROPERTY(EditAnywhere, Category = "Note")
	FLinearColor NoteColor;

	// Not metni ayarlama
	void SetNoteText(const FText& InText);

	// Not boyutu ayarlama
	void SetNoteSize(const FVector2D& InSize);

	// Notun konumunu ve metnini ayarlama
	void SetupNote(const FVector2D& Position, const FText& InText);
}; 
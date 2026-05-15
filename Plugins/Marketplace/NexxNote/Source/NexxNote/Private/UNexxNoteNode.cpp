// Copyright 2025 Onur Altuntaş. All Rights Reserved.
#include "UNexxNoteNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"

UNexxNoteNode::UNexxNoteNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Varsayılan değerler
	NoteText = FText::GetEmpty();
	NoteSize = FVector2D(300.0f, 150.0f);
	
	// Node özellikleri
	NodeWidth = 300.0f;
	NodeHeight = 150.0f;
	
	// Documentation node özellikleri
	Link = FString();
	Excerpt = FString();
	
	// Not özellikleri
	FontSize = 18;
	NoteColor = FLinearColor(1.0f, 0.8f, 0.0f); // Gold color
	
	// NodeGuid'i başlat
	NodeGuid = FGuid::NewGuid();
}

void UNexxNoteNode::PostLoad()
{
    // Üst sınıfın PostLoad metodunu çağır
    Super::PostLoad();
    
    // Dokümantasyon özelliklerini ayarla
    this->Link = FString();
    this->Excerpt = NoteText.ToString();
}

void UNexxNoteNode::AllocateDefaultPins()
{
	// Note node doesn't need pins
}

FText UNexxNoteNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(TEXT("Not"));
}

FLinearColor UNexxNoteNode::GetNodeTitleColor() const
{
	return NoteColor;
}

bool UNexxNoteNode::IsNodePure() const
{
    return false;
}

bool UNexxNoteNode::IsNodeComment() const
{
    return true;
}

bool UNexxNoteNode::ShouldOverridePinNames() const
{
    return false;
}

void UNexxNoteNode::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // Documentation node olduğumuzdan, derleme sırasında herhangi bir işlem yapmamıza gerek yok
    // KismetCompiler bu tür nodeları otomatik olarak atlar
    
    // Basit bir log mesajı bırakıyoruz
    CompilerContext.MessageLog.Note(TEXT("@@ is a note node and will be skipped during compilation"), this);
}

void UNexxNoteNode::SetNoteText(const FText& InText)
{
	if (!NoteText.EqualTo(InText))
	{
		Modify();
		NoteText = InText;
		// Documentation node için Excerpt olarak not metnini ayarla
		Excerpt = NoteText.ToString();
	}
}

void UNexxNoteNode::SetNoteSize(const FVector2D& InSize)
{
	if (NoteSize != InSize)
	{
		Modify();
		NoteSize = InSize;
		NodeWidth = InSize.X;
		NodeHeight = InSize.Y;
	}
}

void UNexxNoteNode::SetupNote(const FVector2D& Position, const FText& InText)
{
	Modify();
	NodePosX = Position.X;
	NodePosY = Position.Y;
	NoteText = InText;
	// Documentation node için Excerpt olarak not metnini ayarla
	Excerpt = NoteText.ToString();
} 
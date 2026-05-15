// Copyright 2025 Onur Altuntaş. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * NexxNote plugin stil sınıfı
 */
class FNexxNoteStyle
{
public:
	/** Stil örneğini başlat */
	static void Initialize();

	/** Stil örneğini kapat */
	static void Shutdown();

	/** Stil örneğinin adını alır */
	static FName GetStyleSetName();

	/** Stil örneğinin referansını alır */
	static const ISlateStyle& Get();

	/** Style başlatıldı mı kontrolü */
	static bool IsInitialized()
	{
		return StyleInstance.IsValid();
	}

private:
	/** Stil örneğini oluştur */
	static TSharedRef<FSlateStyleSet> Create();

	/** Stil örneği */
	static TSharedPtr<FSlateStyleSet> StyleInstance;
}; 
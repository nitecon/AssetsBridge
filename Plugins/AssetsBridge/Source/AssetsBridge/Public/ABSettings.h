// Copyright 2023 Nitecon Studios LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ABSettings.generated.h"

/**
 * 
 */
UCLASS(config = AssetsBridge)
class ASSETSBRIDGE_API UABSettings : public UObject
{
	GENERATED_BODY()

public:
	UABSettings(const FObjectInitializer& obj);

	/** Root directory on disk where assets are exported to/imported from */
	UPROPERTY(Config, EditAnywhere, Category = "Assets Bridge Configuration")
	FString AssetLocationOnDisk;
};

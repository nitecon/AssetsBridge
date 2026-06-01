// Copyright 2023 Nitecon Studios LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetsBridgeTools.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PBRMaterialBuilder.generated.h"

class UTexture2D;
class UMaterialInstanceConstant;

/** Which master-material slot a baked texture feeds (drives sRGB / compression settings). */
UENUM()
enum class EPBRTextureRole : uint8
{
	BaseColor,
	ORM,
	Normal,
	Emissive
};

/**
 * Imports baked PBR textures from disk and builds a Material Instance of the project
 * master material (M_ORM), wiring BaseColor / MRAO / Normal / Emissive Mask parameters.
 * Editor-only.
 */
UCLASS()
class ASSETSBRIDGE_API UPBRMaterialBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Import a PNG from disk into a UTexture2D at TargetContentFolder, applying role-appropriate
	 * sRGB / compression settings. Returns nullptr on failure.
	 */
	static UTexture2D* ImportTexture(const FString& DiskFile, const FString& TargetContentFolder,
	                                 EPBRTextureRole Role, FString& OutMessage);

	/**
	 * Build (or update) MI_<ShortName> parented to MasterPath, importing the texture set and
	 * setting BaseColor / MRAO / Normal / 'Emissive Mask' parameters. Returns the instance.
	 *
	 * @param Set                 The baked texture set (disk paths + content paths).
	 * @param ShortName           Asset short name used for MI naming and default texture folder.
	 * @param FallbackContentDir  Content folder used when texture/MI content paths are blank.
	 * @param MasterPathOverride  Optional master path; falls back to Set.Master then a default.
	 */
	static UMaterialInstanceConstant* BuildMaterialInstance(const FBridgeTextureSet& Set,
	                                                         const FString& ShortName,
	                                                         const FString& FallbackContentDir,
	                                                         const FString& MasterPathOverride,
	                                                         FString& OutMessage);
};

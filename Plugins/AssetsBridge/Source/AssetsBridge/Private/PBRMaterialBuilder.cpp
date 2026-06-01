// Copyright 2023 Nitecon Studios LLC. All rights reserved.

#include "PBRMaterialBuilder.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialParameters.h"
#include "UObject/SavePackage.h"
#include "Misc/Paths.h"
#endif

// Default master material when neither the manifest nor the caller specify one.
static const TCHAR* GDefaultMasterPath = TEXT("/Game/Materials/_Core/M_ORM.M_ORM");

#if WITH_EDITOR
static void ApplyTextureRoleSettings(UTexture2D* Tex, EPBRTextureRole Role)
{
	if (!Tex)
	{
		return;
	}
	switch (Role)
	{
	case EPBRTextureRole::BaseColor:
		Tex->SRGB = true;
		Tex->CompressionSettings = TC_Default;
		break;
	case EPBRTextureRole::ORM:
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_Masks;
		break;
	case EPBRTextureRole::Normal:
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_Normalmap;
		Tex->LODGroup = TEXTUREGROUP_WorldNormalMap;
		break;
	case EPBRTextureRole::Emissive:
		Tex->SRGB = true;
		Tex->CompressionSettings = TC_Default;
		break;
	}
	// The master material (M_ORM) samples every map as a Virtual Texture
	// (SAMPLERTYPE_Virtual*), so the imported textures must be VT-streaming or the
	// sampler reports "requires virtual texture" and the binding is invalid.
	Tex->VirtualTextureStreaming = true;
	Tex->UpdateResource();
	Tex->PostEditChange();
	Tex->MarkPackageDirty();
}
#endif

UTexture2D* UPBRMaterialBuilder::ImportTexture(const FString& DiskFile, const FString& TargetContentFolder,
                                               EPBRTextureRole Role, FString& OutMessage)
{
#if WITH_EDITOR
	if (DiskFile.IsEmpty() || !FPaths::FileExists(DiskFile))
	{
		OutMessage = FString::Printf(TEXT("Texture file missing: %s"), *DiskFile);
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = DiskFile;
	Task->DestinationPath = TargetContentFolder;
	Task->DestinationName = FPaths::GetBaseFilename(DiskFile);
	Task->bSave = false;
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bReplaceExistingSettings = false;

	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task);
	AssetToolsModule.Get().ImportAssetTasks(Tasks);

	UTexture2D* Tex = nullptr;
	for (UObject* Obj : Task->GetObjects())
	{
		Tex = Cast<UTexture2D>(Obj);
		if (Tex)
		{
			break;
		}
	}

	if (!Tex)
	{
		OutMessage = FString::Printf(TEXT("Import produced no texture for %s"), *DiskFile);
		return nullptr;
	}

	ApplyTextureRoleSettings(Tex, Role);
	OutMessage = FString::Printf(TEXT("Imported %s"), *Tex->GetPathName());
	return Tex;
#else
	OutMessage = TEXT("Texture import is editor-only.");
	return nullptr;
#endif
}

UMaterialInstanceConstant* UPBRMaterialBuilder::BuildMaterialInstance(const FBridgeTextureSet& Set,
                                                                      const FString& ShortName,
                                                                      const FString& FallbackContentDir,
                                                                      const FString& MasterPathOverride,
                                                                      FString& OutMessage)
{
#if WITH_EDITOR
	// Resolve the master material.
	FString MasterPath = MasterPathOverride;
	if (MasterPath.IsEmpty())
	{
		MasterPath = Set.Master.IsEmpty() ? FString(GDefaultMasterPath) : Set.Master;
	}
	UMaterial* Master = LoadObject<UMaterial>(nullptr, *MasterPath);
	if (!Master)
	{
		OutMessage = FString::Printf(TEXT("Master material not found: %s"), *MasterPath);
		return nullptr;
	}

	// Resolve content folder for textures + MI (prefer manifest content paths, else fallback).
	auto FolderOf = [](const FString& ContentPath, const FString& Fallback) -> FString
	{
		return ContentPath.IsEmpty() ? Fallback : ContentPath;
	};
	const FString BaseFolder = FolderOf(Set.BaseColor.ContentPath, FallbackContentDir);
	const FString OrmFolder = FolderOf(Set.Orm.ContentPath, FallbackContentDir);
	const FString NormalFolder = FolderOf(Set.Normal.ContentPath, FallbackContentDir);
	const FString EmissiveFolder = FolderOf(Set.Emissive.ContentPath, FallbackContentDir);

	// Import textures (skip blanks; null textures simply leave master defaults in place).
	FString Msg;
	UTexture2D* BaseTex = Set.BaseColor.File.IsEmpty() ? nullptr : ImportTexture(Set.BaseColor.File, BaseFolder, EPBRTextureRole::BaseColor, Msg);
	UTexture2D* OrmTex = Set.Orm.File.IsEmpty() ? nullptr : ImportTexture(Set.Orm.File, OrmFolder, EPBRTextureRole::ORM, Msg);
	UTexture2D* NormalTex = Set.Normal.File.IsEmpty() ? nullptr : ImportTexture(Set.Normal.File, NormalFolder, EPBRTextureRole::Normal, Msg);
	UTexture2D* EmissiveTex = Set.Emissive.File.IsEmpty() ? nullptr : ImportTexture(Set.Emissive.File, EmissiveFolder, EPBRTextureRole::Emissive, Msg);

	// Resolve the MI package path + name.
	FString MIObjectPath = Set.MaterialInstance;
	FString MIPackagePath;
	FString MIName;
	if (!MIObjectPath.IsEmpty())
	{
		MIPackagePath = FPackageName::GetLongPackagePath(MIObjectPath);
		MIName = FPackageName::GetShortName(MIObjectPath);
	}
	else
	{
		MIPackagePath = FallbackContentDir;
		MIName = FString("MI_") + ShortName;
	}

	// Load existing MI or create a new one parented to the master.
	const FString MIFullPath = MIPackagePath / MIName + TEXT(".") + MIName;
	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *MIFullPath);
	if (MIC)
	{
		MIC->SetParentEditorOnly(Master);
	}
	else
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = Master;
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(MIName, MIPackagePath,
			UMaterialInstanceConstant::StaticClass(), Factory);
		MIC = Cast<UMaterialInstanceConstant>(NewAsset);
	}

	if (!MIC)
	{
		OutMessage = FString::Printf(TEXT("Failed to create material instance %s"), *MIFullPath);
		return nullptr;
	}

	// Wire texture parameters (only those that imported; nulls keep master defaults).
	// Use the engine-level editor-only setter (no MaterialEditor module dependency).
	auto SetTex = [MIC](const TCHAR* Param, UTexture2D* Tex)
	{
		if (Tex)
		{
			MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(FName(Param)), Tex);
		}
	};
	SetTex(TEXT("BaseColor"), BaseTex);
	SetTex(TEXT("ORM"), OrmTex);
	SetTex(TEXT("Normal"), NormalTex);
	SetTex(TEXT("Emissive Mask"), EmissiveTex);

	// When an emissive map was baked, turn on the master's emissive path (it defaults off)
	// and give it a sensible starting intensity so the glow actually shows. The artist can
	// tune 'Emissive Intensity' afterwards; re-imports preserve nothing here, so we re-apply.
	if (EmissiveTex)
	{
		MIC->SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(FName("Use Emissive Mask")), true);
		MIC->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(FName("Emissive Intensity")), 2.0f);
	}

	MIC->PostEditChange();
	MIC->MarkPackageDirty();

	OutMessage = FString::Printf(TEXT("Built material instance %s (parent %s)"), *MIC->GetPathName(), *MasterPath);
	return MIC;
#else
	OutMessage = TEXT("Material instance creation is editor-only.");
	return nullptr;
#endif
}

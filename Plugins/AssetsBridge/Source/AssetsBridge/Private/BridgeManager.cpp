// Copyright 2023 Nitecon Studios LLC. All rights reserved.


#include "BridgeManager.h"

#include "AssetsBridgeTools.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBlueprint.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageTools.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Exporters/Exporter.h"
#include "Exporters/FbxExportOption.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Materials/MaterialInstance.h"
// FBX Exporter is in UnrealEd private - use full path
THIRD_PARTY_INCLUDES_START
#include "Editor/UnrealEd/Private/FbxExporter.h"
THIRD_PARTY_INCLUDES_END
#include "Editor/UnrealEd/Public/AssetImportTask.h"
#include "AssetToolsModule.h"
#include "AutomatedAssetImportData.h"
#include "Subsystems/EditorActorSubsystem.h"

UBridgeManager::UBridgeManager()
{
}

void UBridgeManager::ExecuteSwap(TArray<AActor*> SelectList, TArray<FAssetData> ContentList, bool& bIsSuccessful, FString& OutMessage)
{
	if (SelectList.Num() < 1)
	{
		bIsSuccessful = false;
		OutMessage = "You must select at least 1 item in the level";
		return;
	}
	if (ContentList.Num() < 1)
	{
		bIsSuccessful = false;
		OutMessage = "You must select at least 1 from the content browser to replace the selected items with";
		return;
	}
	for (auto Asset : ContentList)
	{
		if (UClass* AssetClass = Asset.GetClass())
		{
			UActorFactory* Factory = nullptr;

			if (AssetClass->IsChildOf(UBlueprint::StaticClass()))
			{
				Factory = GEditor->FindActorFactoryByClass(UActorFactoryBlueprint::StaticClass());
			}
			else
			{
				const TArray<UActorFactory*>& ActorFactories = GEditor->ActorFactories;
				for (int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); FactoryIdx++)
				{
					UActorFactory* ActorFactory = ActorFactories[FactoryIdx];
					// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
					FText ErrorMessage;
					if (ActorFactory->CanCreateActorFrom(Asset, ErrorMessage))
					{
						Factory = ActorFactory;
						break;
					}
				}
			}

			if (Factory)
			{
				// Use new UE5 API instead of deprecated GEditor->ReplaceSelectedActors
				if (UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
				{
					EditorActorSubsystem->ReplaceSelectedActors(Factory, Asset);
				}
			}
		}
	}
	bIsSuccessful = true;
	OutMessage = "Operation Succeeded.";
}

bool UBridgeManager::IsSystemPath(FString Path)
{
	if (Path.StartsWith("/Engine"))
	{
		return true;
	}
	return false;
}


FExportAsset UBridgeManager::DuplicateAndSwap(FExportAsset InAsset, bool& bIsSuccessful, FString& OutMessage)
{
	FExportAsset OutAsset;
	UStaticMesh* Mesh = Cast<UStaticMesh>(InAsset.ModelPtr);
	if (Mesh)
	{
		FString SourcePackagePath = UAssetsBridgeTools::GetPathWithoutExt(Mesh->GetPathName());
		FString TargetPath = UAssetsBridgeTools::GetSystemPathAsAssetPath(SourcePackagePath);
		UObject* DuplicateObject = UEditorAssetLibrary::DuplicateAsset(SourcePackagePath, TargetPath);
		if (DuplicateObject == nullptr)
		{
			bIsSuccessful = false;
			OutMessage = FString::Printf(TEXT("Cannot duplicate: %s to %s, does it already exist?"), *SourcePackagePath, *TargetPath);
			return {};
		}
		UStaticMesh* DuplicateMesh = Cast<UStaticMesh>(DuplicateObject);
		if (DuplicateMesh)
		{
			OutAsset.ModelPtr = DuplicateMesh;
			OutAsset.InternalPath = UAssetsBridgeTools::GetPathWithoutExt(DuplicateMesh->GetPathName()).Replace(TEXT("/Game"), TEXT(""));
			OutAsset.ShortName = UAssetsBridgeTools::GetPathWithoutExt(DuplicateMesh->GetName());
			TArray<FMaterialSlot> DupeMats;
			// now we duplicate each of the materials:
			auto StaticMats = Mesh->GetStaticMaterials();
			for (FStaticMaterial SrcMat : StaticMats)
			{
				FMaterialSlot DupeMaterial;
				FString SourceMaterialPath = UAssetsBridgeTools::GetPathWithoutExt(SrcMat.MaterialInterface->GetPathName());
				auto MatIdx = Mesh->GetMaterialIndex(FName(SrcMat.MaterialSlotName));
				DupeMaterial.Idx = MatIdx;
				DupeMaterial.Name = SrcMat.MaterialSlotName.ToString();
				FString TargetMatPath = UAssetsBridgeTools::GetSystemPathAsAssetPath(SourceMaterialPath);
				UObject* DuplicateMat = UEditorAssetLibrary::DuplicateAsset(SourceMaterialPath, TargetMatPath);
				if (DuplicateMat == nullptr)
				{
					bIsSuccessful = false;
					OutMessage = FString::Printf(TEXT("Cannot duplicate: %s to %s, does it already exist?"), *SourcePackagePath, *TargetPath);
					return {};
				}
				UMaterialInstance* NewMat = Cast<UMaterialInstance>(DuplicateMat);
				if (NewMat != nullptr)
				{
					DuplicateMesh->SetMaterial(MatIdx, NewMat);
					DupeMaterial.InternalPath = UAssetsBridgeTools::GetPathWithoutExt(NewMat->GetPathName());
				}
				DupeMats.Add(DupeMaterial);
			}
			OutAsset.ModelPtr = DuplicateMesh;
		}
		FAssetData AssetData = UAssetsBridgeTools::GetAssetDataFromPath(DuplicateMesh->GetPathName());
		TArray<FAssetData> AssetItems;
		AssetItems.Add(AssetData);
		ExecuteSwap(UAssetsBridgeTools::GetWorldSelection(), AssetItems, bIsSuccessful, OutMessage);
	}

	return OutAsset;
}

bool UBridgeManager::HasMatchingExport(TArray<FExportAsset> Assets, FAssetData InAsset)
{
	for (FExportAsset ExAsset : Assets)
	{
		if (ExAsset.ModelPtr && ExAsset.ModelPtr->GetPathName().Equals(InAsset.GetAsset()->GetPathName()))
		{
			return true;
		}
	}
	return false;
}

FString UBridgeManager::ComputeTransformChecksum(FWorldData& Object)
{
	// Serialize the object data to a memory buffer
	TArray<uint8> ObjectData;
	FMemoryWriter Writer(ObjectData);
	Object.Serialize(Writer);

	// Compute the SHA-1 hash of the object data
	FSHA1 Sha1;
	Sha1.Update(ObjectData.GetData(), ObjectData.Num());
	Sha1.Final();
	uint8 Hash[FSHA1::DigestSize];
	Sha1.GetHash(Hash);

	// Convert the hash to a hexadecimal string
	FString HexHash;
	for (int i = 0; i < FSHA1::DigestSize; i++)
	{
		HexHash += FString::Printf(TEXT("%02x"), Hash[i]);
	}

	return HexHash;
}

void UBridgeManager::StartExport(bool& bIsSuccessful, FString& OutMessage)
{
	TArray<FExportAsset> ExportArray;
	TArray<FAssetData> SelectedAssets;
	UAssetsBridgeTools::GetSelectedContentBrowserItems(SelectedAssets);
	TArray<FAssetDetails> Selection = UAssetsBridgeTools::GetWorldSelectedAssets();
	if (Selection.Num() == 0 && SelectedAssets.Num() == 0)
	{
		bIsSuccessful = false;
		OutMessage = FString(TEXT("Please select at least one item in the level / content browser to export."));
		return;
	}
	if (Selection.Num() > 0)
	{
		for (auto SelItem : Selection)
		{
			//SelectedAsset.ObjectAsset.PackagePath.ToString())
			FExportAsset ExpItem = UAssetsBridgeTools::GetExportInfo(SelItem.ObjectAsset, bIsSuccessful, OutMessage);
			if (!bIsSuccessful)
			{
				return;
			}
			AActor* ItemActor = Cast<AActor>(SelItem.WorldObject);
			if (ItemActor)
			{
				UE_LOG(LogTemp, Warning, TEXT("Found this to be an actual world actor"))
				FRotator Rotator = ItemActor->GetActorTransform().GetRotation().Rotator();
				FWorldData ActorWorldInfo;
				ActorWorldInfo.Location = ItemActor->GetActorLocation();
				ActorWorldInfo.Rotation = FVector(Rotator.Roll, Rotator.Pitch, Rotator.Yaw);
				ActorWorldInfo.Scale = ItemActor->GetActorScale();
				ExpItem.WorldData = ActorWorldInfo;
				// Create a checksum from the world data to set as ObjectID
				//ExpItem.ObjectID = ComputeTransformChecksum(ExpItem.WorldData);
				ExpItem.ObjectID = ItemActor->GetName();
				//ExpItem.ObjectID = 
			}
			ExportArray.Add(ExpItem);
		}
		// we only have world selections so convert to assets and export with world context
	}
	if (SelectedAssets.Num() > 0)
	{
		for (auto CAsset : SelectedAssets)
		{
			// If a content browser item matches an item in the export array we can skip it as it should be the same item with world context. else add
			if (!HasMatchingExport(ExportArray, CAsset))
			{
				ExportArray.Add(UAssetsBridgeTools::GetExportInfo(CAsset, bIsSuccessful, OutMessage));
				if (!bIsSuccessful)
				{
					return;
				}
			}
		}
	}

	if (bIsSuccessful)
	{
		/*for (auto ExItem : ExportArray)
		{
			// Create a formatted FString
			FString InternalPath = FString::Printf(TEXT("/Game%s/%s"), *ExItem.InternalPath, *ExItem.ShortName);
			UE_LOG(LogTemp, Warning, TEXT("Exporting %s to %s"), *InternalPath, *ExItem.ExportLocation);
			ExportObject(InternalPath, ExItem.ExportLocation, bIsSuccessful, OutMessage);
			if (!bIsSuccessful)
			{
				return;
			}
		}*/
		GenerateExport(ExportArray, bIsSuccessful, OutMessage);
	}
}


void UBridgeManager::GenerateExport(TArray<FExportAsset> MeshDataArray, bool& bIsSuccessful, FString& OutMessage)
{
	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	bool bIsCanceled = false;
	bool bExportAll;
	INodeNameAdapter NodeNameAdapter;
	Exporter->FillExportOptions(false, false, UExporter::CurrentFilename, bIsCanceled, bExportAll);
	UFbxExportOption* ExportOptions = Exporter->GetExportOptions();
	ExportOptions->FbxExportCompatibility = EFbxExportCompatibility::FBX_2020;
	ExportOptions->bForceFrontXAxis = false;
	ExportOptions->bASCII = false;
	ExportOptions->LevelOfDetail = false;
	ExportOptions->SaveOptions();
	Exporter->SetExportOptionsOverride(ExportOptions);
	FBridgeExport ExportData;
	ExportData.Operation = "UnrealExport";
	for (auto Item : MeshDataArray)
	{
		bool bDidExport = false;
		//FBridgeExportElement ExItem;
		// Create the distination directory if it doesn't already exist
		FString ItemPath = FPaths::GetPath(*Item.ExportLocation);
		if (!IFileManager::Get().DirectoryExists(*ItemPath))
		{
			const bool bTree = true;
			if (!IFileManager::Get().MakeDirectory(*ItemPath, bTree))
			{
				UE_LOG(LogTemp, Error, TEXT("%s. The destination directory could not be created."), *ItemPath);
				bIsSuccessful = false;
				OutMessage = FString::Printf(TEXT("%s. The destination directory could not be created."), *ItemPath);
				return;
			}
		}
		// TODO: just use Item.ModelPtr to run export task in the future.
		UStaticMesh* Mesh = Cast<UStaticMesh>(Item.ModelPtr);
		if (Mesh != nullptr)
		{
			Exporter->CreateDocument();
			Exporter->ExportStaticMesh(Mesh, &Mesh->GetStaticMaterials());
			Exporter->WriteToFile(*Item.ExportLocation);
			Exporter->CloseDocument();
			//UGLTFExporter::ExportToGLTF(Mesh, *Item.ExportLocation.Replace(TEXT(".fbx"), TEXT(".gltf")));
			bDidExport = true;
		}
		USkeletalMesh* SkeleMesh = Cast<USkeletalMesh>(Item.ModelPtr);
		if (SkeleMesh != nullptr)
		{
			Exporter->CreateDocument();
			Exporter->ExportSkeletalMesh(SkeleMesh);
			Exporter->WriteToFile(*Item.ExportLocation);
			Exporter->CloseDocument();
			//UGLTFExporter::ExportToGLTF(Mesh, *Item.ExportLocation.Replace(TEXT(".fbx"), TEXT(".gltf")));
			bDidExport = true;
		}
		if (bDidExport)
		{
			ExportData.Objects.Add(Item);
		}
	}
	Exporter->DeleteInstance();
	UAssetsBridgeTools::WriteBridgeExportFile(ExportData, bIsSuccessful, OutMessage);
}


void UBridgeManager::GenerateImport(bool& bIsSuccessful, FString& OutMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("Starting import"))
	FBridgeExport BridgeData = UAssetsBridgeTools::ReadBridgeExportFile(bIsSuccessful, OutMessage);
	if (!bIsSuccessful)
	{
		return;
	}

	for (auto Item : BridgeData.Objects)
	{
		// Try to extract original asset name from 'Model' field which contains the full path
		// Format: "/Script/Engine.SkeletalMesh'/Game/Path/AssetName.AssetName'" or "/Game/Path/AssetName.AssetName"
		FString OriginalName = Item.ShortName;
		if (!Item.Model.IsEmpty())
		{
			// Extract asset name from model path
			FString ModelPathStr = Item.Model;
			int32 LastSlash = ModelPathStr.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			int32 FirstDot = ModelPathStr.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart, LastSlash);
			if (LastSlash != INDEX_NONE && FirstDot != INDEX_NONE)
			{
				OriginalName = ModelPathStr.Mid(LastSlash + 1, FirstDot - LastSlash - 1);
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Extracted original name '%s' from ModelPath"), *OriginalName);
			}
		}
		
		// Normalize the internal path
		FString NormalizedPath = Item.InternalPath;
		
		// Remove any /Game or /Content prefix if included
		NormalizedPath.RemoveFromStart(TEXT("/Game"));
		NormalizedPath.RemoveFromStart(TEXT("Game"));
		NormalizedPath.RemoveFromStart(TEXT("/Content"));
		NormalizedPath.RemoveFromStart(TEXT("Content"));
		
		// Ensure leading slash
		if (!NormalizedPath.StartsWith("/"))
		{
			NormalizedPath = "/" + NormalizedPath;
		}
		
		// Fix doubled path segments (e.g., /Assets/Assets/ -> /Assets/)
		TArray<FString> PathSegments;
		NormalizedPath.ParseIntoArray(PathSegments, TEXT("/"), true);
		if (PathSegments.Num() >= 2 && PathSegments[0] == PathSegments[1])
		{
			PathSegments.RemoveAt(0);
			NormalizedPath = "/" + FString::Join(PathSegments, TEXT("/"));
			UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Fixed doubled path segment, normalized to: %s"), *NormalizedPath);
		}
		
		FString ImportPackageName = FString("/Game") + NormalizedPath + FString("/") + OriginalName;
		ImportPackageName = UPackageTools::SanitizePackageName(ImportPackageName);
		bool bHasExisting = false;
		FString ExistingName;
		if (HasExistingPackageAtPath(ImportPackageName))
		{
			UStaticMesh* ExistingMesh = FindObject<UStaticMesh>(nullptr, *ImportPackageName);
			if (ExistingMesh != nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("Found existing mesh, closing all related editors"))
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(ExistingMesh);
			}

			bHasExisting = true;
		}
		ImportAsset(Item.ExportLocation, ImportPackageName, Item.StringType, Item.Skeleton, bIsSuccessful, OutMessage);
		if (!bIsSuccessful)
		{
			return;
		}
	}
	bIsSuccessful = true;

	OutMessage = FString::Printf(TEXT("Operation was successful"));
}

void UBridgeManager::ReplaceRefs(FString OldPackageName, UPackage* NewPackage, bool& bIsSuccessful, FString& OutMessage)
{
	// move assets from the old package to the new package
	TArray<UObject*> Assets;
	GetObjectsWithOuter(FindPackage(nullptr, *OldPackageName), Assets);
	for (UObject* Asset : Assets)
	{
		Asset->Rename(nullptr, NewPackage, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
	}

	// replace all references to the old package with references to the new package
	TArray<UObject*> Objects;
	GetObjectsOfClass(UObject::StaticClass(), Objects);
	if (Objects.Num() > 0)
	{
		for (UObject* Obj : Objects)
		{
			if (Obj != nullptr && Obj->GetOuter() != nullptr)
			{
				if (Obj->GetOuter()->GetName() == OldPackageName)
				{
					Obj->Rename(nullptr, NewPackage, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
				}
			}
		}
	}
	// remove the old package from the asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistrySingleton = AssetRegistryModule.Get();
	TArray<FAssetData> AssetsData;
	AssetRegistrySingleton.GetAssetsByPackageName(*OldPackageName, AssetsData);
	for (const FAssetData& Asset : AssetsData)
	{
		bIsSuccessful = UEditorAssetLibrary::DeleteAsset(Asset.GetObjectPathString());
		if (!bIsSuccessful)
		{
			OutMessage = "Could not delete asset";
			return;
		}
	}
	bIsSuccessful = true;
	OutMessage = "References Replaced";
}

bool UBridgeManager::HasExistingPackageAtPath(FString InPath)
{
	const FString PackageName = FPackageName::ObjectPathToPackageName(InPath);
	return FPackageName::DoesPackageExist(PackageName);
}

UObject* UBridgeManager::ImportAsset(FString InSourcePath, FString InDestPath, FString InMeshType, FString InSkeletonPath, bool& bIsSuccessful, FString& OutMessage)
{
	UAssetImportTask* ImportTask = CreateImportTask(InSourcePath, InDestPath, InMeshType, InSkeletonPath, bIsSuccessful, OutMessage);
	if (!bIsSuccessful)
	{
		return nullptr;
	}
	UObject* RetAss = ProcessTask(ImportTask, bIsSuccessful, OutMessage);
	if (!bIsSuccessful)
	{
		return nullptr;
	}
	bIsSuccessful = true;
	OutMessage = "Asset Imported";
	return RetAss;
}

UObject* UBridgeManager::ProcessTask(UAssetImportTask* ImportTask, bool& bIsSuccessful, FString& OutMessage)
{
	if (ImportTask == nullptr)
	{
		bIsSuccessful = false;
		OutMessage = "Could not process task";
		return nullptr;
	}
	FAssetToolsModule* AssetTools = FModuleManager::LoadModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetTools == nullptr)
	{
		bIsSuccessful = false;
		OutMessage = "Could not load asset tools module";
		return nullptr;
	}
	AssetTools->Get().ImportAssetTasks({ImportTask});
	if (ImportTask->GetObjects().Num() == 0)
	{
		bIsSuccessful = false;
		OutMessage = "Could not process task";
		return nullptr;
	}
	UObject* ImportedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FPaths::Combine(ImportTask->DestinationPath, ImportTask->DestinationName));
	if (ImportedObject == nullptr)
	{
		bIsSuccessful = false;
		OutMessage = "Import partially successful but returned invalid object";
		return nullptr;
	}
	bIsSuccessful = true;
	OutMessage = "Import success";
	return ImportedObject;
}

UAssetImportTask* UBridgeManager::CreateImportTask(FString InSourcePath, FString InDestPath, FString InMeshType,
                                                   FString InSkeletonPath, bool& bIsSuccessful, FString& OutMessage)
{
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: === CreateImportTask ==="));
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Source: %s"), *InSourcePath);
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Dest: %s"), *InDestPath);
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: MeshType: %s"), *InMeshType);
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: SkeletonPath: %s"), *InSkeletonPath);
	
	UAssetImportTask* ResTask = NewObject<UAssetImportTask>();
	if (ResTask == nullptr)
	{
		bIsSuccessful = false;
		OutMessage = "Could not create asset import task";
		return nullptr;
	}
	ResTask->Filename = InSourcePath;
	ResTask->DestinationPath = FPaths::GetPath(InDestPath);
	ResTask->DestinationName = FPaths::GetCleanFilename(InDestPath);

	ResTask->bSave = false;
	ResTask->bAutomated = true;
	ResTask->bAsync = false;
	ResTask->bReplaceExisting = true;
	ResTask->bReplaceExistingSettings = false;

	// Configure FBX factory with proper settings - forces legacy FBX importer instead of Interchange
	UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
	FbxFactory->AddToRoot();
	
	// CRITICAL: Set this to prevent Interchange from taking over the import
	FbxFactory->SetAutomatedAssetImportData(NewObject<UAutomatedAssetImportData>());
	
	// Common settings - don't import materials/textures (use existing ones)
	FbxFactory->ImportUI->bImportMaterials = false;
	FbxFactory->ImportUI->bImportTextures = false;
	FbxFactory->ImportUI->bImportAnimations = false;
	FbxFactory->ImportUI->bAutomatedImportShouldDetectType = false;
	
	const bool bIsSkeletalMesh = InMeshType.Equals(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase);
	
	if (bIsSkeletalMesh)
	{
		// Skeletal mesh specific settings
		FbxFactory->ImportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
		FbxFactory->ImportUI->bImportAsSkeletal = true;
		FbxFactory->ImportUI->bImportMesh = true;
		
		// NOTE: We intentionally do NOT force a skeleton here.
		// The "cannot merge bone tree" error occurs when the FBX has a different bone structure
		// than the skeleton we're trying to use. Let Interchange/FBX handle skeleton assignment.
		// If the same mesh exists at destination, Unreal will update it properly.
		// If not, a new skeleton will be created.
		
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: SkeletonPath from JSON: %s"), *InSkeletonPath);
		
		// Check if destination mesh exists - just for logging
		FString FullAssetPath = InDestPath + TEXT(".") + FPaths::GetBaseFilename(InDestPath);
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Looking for existing mesh at: %s"), *FullAssetPath);
		USkeletalMesh* ExistingMesh = FindObject<USkeletalMesh>(nullptr, *FullAssetPath);
		if (ExistingMesh)
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Found existing mesh: %s"), *ExistingMesh->GetPathName());
			if (ExistingMesh->GetSkeleton())
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Existing mesh skeleton: %s"), *ExistingMesh->GetSkeleton()->GetPathName());
			}
			// Mark as reimport so it updates the existing asset
			FbxFactory->ImportUI->bIsReimport = true;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: No existing mesh found - new import"));
			FbxFactory->ImportUI->bIsReimport = false;
		}
		
		// Don't force skeleton - let the import system handle it based on the FBX content
		FbxFactory->ImportUI->Skeleton = nullptr;
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Skeleton not forced - letting import system handle it"));
		
		// Skeletal mesh import settings
		FbxFactory->ImportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy = true;
		FbxFactory->ImportUI->SkeletalMeshImportData->bConvertScene = true;
		FbxFactory->ImportUI->SkeletalMeshImportData->bForceFrontXAxis = false;
		FbxFactory->ImportUI->SkeletalMeshImportData->bConvertSceneUnit = true;
		// DON'T use T0 as reference pose - use the actual bind pose from the FBX
		FbxFactory->ImportUI->SkeletalMeshImportData->bUseT0AsRefPose = false;
		// Preserve smoothing groups
		FbxFactory->ImportUI->SkeletalMeshImportData->bPreserveSmoothingGroups = true;
		// Keep bind pose from FBX
		FbxFactory->ImportUI->SkeletalMeshImportData->bImportMorphTargets = true;
		
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Skeletal mesh settings - bUseT0AsRefPose=false, bConvertScene=true"));
	}
	else
	{
		// Static mesh settings
		FbxFactory->ImportUI->MeshTypeToImport = FBXIT_StaticMesh;
		FbxFactory->ImportUI->bImportAsSkeletal = false;
		FbxFactory->ImportUI->bImportMesh = true;
		FbxFactory->ImportUI->StaticMeshImportData->bCombineMeshes = true;
	}
	
	ResTask->Factory = FbxFactory;
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Import task configured:"));
	UE_LOG(LogTemp, Log, TEXT("  - Factory: %s"), *FbxFactory->GetClass()->GetName());
	UE_LOG(LogTemp, Log, TEXT("  - bReplaceExisting: %s"), ResTask->bReplaceExisting ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("  - bAutomated: %s"), ResTask->bAutomated ? TEXT("true") : TEXT("false"));
	if (FbxFactory->ImportUI->Skeleton)
	{
		UE_LOG(LogTemp, Log, TEXT("  - Skeleton: %s"), *FbxFactory->ImportUI->Skeleton->GetPathName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("  - Skeleton: nullptr"));
	}

	bIsSuccessful = true;
	OutMessage = "Task Created";
	return ResTask;
}

void UBridgeManager::ExportObject(FString InObjInternalPath, FString InDestPath, bool& bIsSuccessful, FString& OutMessage)
{
	FAssetToolsModule* AssetTools = FModuleManager::LoadModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetTools == nullptr)
	{
		bIsSuccessful = false;
		OutMessage = "Could not load asset tools module";
		AssetTools = nullptr;
		return;
	}
	AssetTools->Get().ExportAssets(TArray<FString>{InObjInternalPath}, FPaths::GetPath(InDestPath));
	bIsSuccessful = true;
	OutMessage = "Export success";
}

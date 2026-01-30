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
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "Exporters/Exporter.h"
#include "Materials/MaterialInstance.h"
#include "Editor/UnrealEd/Public/AssetImportTask.h"
#include "AssetToolsModule.h"
#include "AutomatedAssetImportData.h"
#include "Subsystems/EditorActorSubsystem.h"
// Export task for automated export
#include "AssetExportTask.h"
// Physics asset for retargeting
#include "PhysicsEngine/PhysicsAsset.h"
// For skeleton compatibility check
#include "Engine/SkinnedAssetCommon.h"
// For iterating world actors and updating mesh components
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

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
	FBridgeExport ExportData;
	ExportData.Operation = "UnrealExport";
	
	// Find the glTF exporter class
	UClass* GLTFExporterClass = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UExporter::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			FString ClassName = It->GetName();
			if (ClassName.Contains(TEXT("GLTFStaticMeshExporter")) || ClassName.Contains(TEXT("GLTFSkeletalMeshExporter")))
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Found glTF exporter class: %s"), *ClassName);
			}
		}
	}
	
	for (auto Item : MeshDataArray)
	{
		bool bDidExport = false;
		
		// Create the destination directory if it doesn't already exist
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
		
		UObject* ObjectToExport = nullptr;
		FString ExporterClassName;
		
		UStaticMesh* Mesh = Cast<UStaticMesh>(Item.ModelPtr);
		if (Mesh != nullptr)
		{
			ObjectToExport = Mesh;
			ExporterClassName = TEXT("GLTFStaticMeshExporter");
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Preparing to export static mesh %s to glTF: %s"), *Mesh->GetName(), *Item.ExportLocation);
		}
		
		USkeletalMesh* SkeleMesh = Cast<USkeletalMesh>(Item.ModelPtr);
		if (SkeleMesh != nullptr)
		{
			ObjectToExport = SkeleMesh;
			ExporterClassName = TEXT("GLTFSkeletalMeshExporter");
			
			// Log skeleton info for debugging
			USkeleton* Skeleton = SkeleMesh->GetSkeleton();
			if (Skeleton)
			{
				const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
				int32 NumBones = RefSkeleton.GetNum();
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge Export: Mesh %s uses skeleton %s with %d total bones"), 
					*SkeleMesh->GetName(), *Skeleton->GetName(), NumBones);
			}
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Preparing to export skeletal mesh %s to glTF: %s"), *SkeleMesh->GetName(), *Item.ExportLocation);
		}
		
		if (ObjectToExport)
		{
			// Find the appropriate glTF exporter
			UExporter* Exporter = nullptr;
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->IsChildOf(UExporter::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
				{
					if (It->GetName().Contains(ExporterClassName))
					{
						Exporter = NewObject<UExporter>(GetTransientPackage(), *It);
						break;
					}
				}
			}
			
			if (Exporter)
			{
				// Use UAssetExportTask for automated export
				UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
				ExportTask->Object = ObjectToExport;
				ExportTask->Exporter = Exporter;
				ExportTask->Filename = Item.ExportLocation;
				ExportTask->bSelected = false;
				ExportTask->bReplaceIdentical = true;
				ExportTask->bPrompt = false;
				ExportTask->bAutomated = true;
				ExportTask->bUseFileArchive = false;
				ExportTask->bWriteEmptyFiles = false;
				
				bool bExportSuccess = UExporter::RunAssetExportTask(ExportTask);
				
				if (bExportSuccess)
				{
					bDidExport = true;
					UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Successfully exported %s"), *ObjectToExport->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Failed to export %s"), *ObjectToExport->GetName());
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("AssetsBridge: Could not find glTF exporter for %s"), *ExporterClassName);
			}
		}
		
		if (bDidExport)
		{
			ExportData.Objects.Add(Item);
		}
	}
	
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
		UObject* ImportedAsset = ImportAsset(Item.ExportLocation, ImportPackageName, Item.StringType, Item.Skeleton, bIsSuccessful, OutMessage);
		if (!bIsSuccessful)
		{
			return;
		}
		
		// Relocate asset if Interchange created it in a subfolder structure
		if (ImportedAsset)
		{
			bool bRelocateSuccess = false;
			FString RelocateMessage;
			UObject* RelocatedAsset = RelocateImportedAsset(ImportedAsset, ImportPackageName, bRelocateSuccess, RelocateMessage);
			if (bRelocateSuccess && RelocatedAsset)
			{
				ImportedAsset = RelocatedAsset;
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: %s"), *RelocateMessage);
			}
			else if (!bRelocateSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Relocation issue: %s"), *RelocateMessage);
				// Continue with original asset even if relocation failed
			}
		}
		
		// Restore morph target names for skeletal meshes
		if (Item.StringType == "SkeletalMesh" && Item.MorphTargets.Num() > 0 && ImportedAsset)
		{
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(ImportedAsset);
			if (SkeletalMesh)
			{
				const TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->GetMorphTargets();
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Restoring %d morph target names (imported has %d)"), 
					Item.MorphTargets.Num(), MorphTargets.Num());
				
				// Rename morph targets to their original names
				int32 NumToRename = FMath::Min(Item.MorphTargets.Num(), MorphTargets.Num());
				for (int32 i = 0; i < NumToRename; i++)
				{
					UMorphTarget* MorphTarget = MorphTargets[i];
					if (MorphTarget)
					{
						FString OldName = MorphTarget->GetName();
						FString NewName = Item.MorphTargets[i];
						if (OldName != NewName)
						{
							MorphTarget->Rename(*NewName, SkeletalMesh, REN_DontCreateRedirectors | REN_NonTransactional);
							UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Renamed morph target %s -> %s"), *OldName, *NewName);
						}
					}
				}
				
				// Mark the mesh as modified so the names are saved
				SkeletalMesh->MarkPackageDirty();
			}
		}
		
		// Note: Automatic skeleton retargeting has been removed.
		// New skeletal mesh imports will keep their own skeleton and physics assets.
		// Users should manually retarget if needed through the Unreal Editor skeleton tools.
		
		// Process material changeset to restore/handle materials
		if (ImportedAsset)
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(ImportedAsset);
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(ImportedAsset);
			
			// Get material counts for bounds checking
			int32 MatCount = StaticMesh ? StaticMesh->GetStaticMaterials().Num() : 
			                 (SkeletalMesh ? SkeletalMesh->GetMaterials().Num() : 0);
			
			// Log changeset info
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Material changeset - Added: %d, Removed: %d, Unchanged: %d"),
				Item.MaterialChangeset.Added.Num(),
				Item.MaterialChangeset.Removed.Num(),
				Item.MaterialChangeset.Unchanged.Num());
			
			// Restore unchanged materials (materials that existed before and still exist)
			for (const FMaterialSlot& MatSlot : Item.MaterialChangeset.Unchanged)
			{
				if (MatSlot.Idx >= MatCount)
				{
					UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Material slot %d out of bounds (mesh has %d slots)"), MatSlot.Idx, MatCount);
					continue;
				}
				
				FString MaterialPath = MatSlot.InternalPath;
				if (!MaterialPath.StartsWith(TEXT("/Game")) && !MaterialPath.StartsWith(TEXT("/Engine")))
				{
					MaterialPath = TEXT("/Game") + MaterialPath;
				}
				
				UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
				if (Material)
				{
					if (StaticMesh)
					{
						StaticMesh->SetMaterial(MatSlot.Idx, Material);
					}
					else if (SkeletalMesh)
					{
						SkeletalMesh->GetMaterials()[MatSlot.Idx].MaterialInterface = Material;
					}
					UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Restored unchanged material %s at slot %d"), *MatSlot.Name, MatSlot.Idx);
				}
			}
			
			// Log added materials (new slots - user needs to assign materials in Unreal)
			for (const FMaterialSlot& MatSlot : Item.MaterialChangeset.Added)
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: New material slot added in Blender: %s at slot %d (assign material in Unreal)"), 
					*MatSlot.Name, MatSlot.Idx);
			}
			
			// Log removed materials
			for (const FMaterialSlot& MatSlot : Item.MaterialChangeset.Removed)
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Material removed in Blender: %s (was at slot %d)"), 
					*MatSlot.Name, MatSlot.OriginalIdx);
			}
			
			// Mark as dirty so changes are saved
			if (StaticMesh)
			{
				StaticMesh->MarkPackageDirty();
			}
			else if (SkeletalMesh)
			{
				SkeletalMesh->MarkPackageDirty();
			}
			
			// Update world actors that use this mesh to refresh their materials
			// This fixes the issue where actors in the viewport lose materials after reimport
			UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (EditorWorld)
			{
				int32 UpdatedActorCount = 0;
				
				if (StaticMesh)
				{
					// Find all actors with StaticMeshComponents using this mesh
					for (TActorIterator<AActor> ActorIt(EditorWorld); ActorIt; ++ActorIt)
					{
						AActor* Actor = *ActorIt;
						if (!Actor) continue;
						
						TArray<UStaticMeshComponent*> MeshComponents;
						Actor->GetComponents(MeshComponents);
						
						for (UStaticMeshComponent* MeshComp : MeshComponents)
						{
							if (MeshComp && MeshComp->GetStaticMesh() == StaticMesh)
							{
								// Clear all material overrides so the component uses the mesh asset's materials
								// This is the key fix - after reimport, the component may have stale overrides
								for (int32 MatIdx = 0; MatIdx < StaticMesh->GetStaticMaterials().Num(); MatIdx++)
								{
									// Setting to nullptr clears the override and uses the mesh asset's material
									MeshComp->SetMaterial(MatIdx, nullptr);
								}
								
								// Force a refresh of the component's rendering
								MeshComp->MarkRenderStateDirty();
								UpdatedActorCount++;
								
								UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Refreshed materials on world actor '%s' (StaticMeshComponent)"), 
									*Actor->GetActorLabel());
							}
						}
					}
				}
				else if (SkeletalMesh)
				{
					// Find all actors with SkeletalMeshComponents using this mesh
					for (TActorIterator<AActor> ActorIt(EditorWorld); ActorIt; ++ActorIt)
					{
						AActor* Actor = *ActorIt;
						if (!Actor) continue;
						
						TArray<USkeletalMeshComponent*> MeshComponents;
						Actor->GetComponents(MeshComponents);
						
						for (USkeletalMeshComponent* MeshComp : MeshComponents)
						{
							if (MeshComp && MeshComp->GetSkeletalMeshAsset() == SkeletalMesh)
							{
								// Clear all material overrides so the component uses the mesh asset's materials
								for (int32 MatIdx = 0; MatIdx < SkeletalMesh->GetMaterials().Num(); MatIdx++)
								{
									MeshComp->SetMaterial(MatIdx, nullptr);
								}
								
								// Force a refresh of the component's rendering
								MeshComp->MarkRenderStateDirty();
								UpdatedActorCount++;
								
								UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Refreshed materials on world actor '%s' (SkeletalMeshComponent)"), 
									*Actor->GetActorLabel());
							}
						}
					}
				}
				
				if (UpdatedActorCount > 0)
				{
					UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Updated materials on %d world actor(s) using mesh '%s'"), 
						UpdatedActorCount, *ImportedAsset->GetName());
				}
			}
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
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: === ImportAsset (glTF) ==="));
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Source: %s"), *InSourcePath);
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Dest: %s"), *InDestPath);
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: MeshType: %s"), *InMeshType);
	
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
	
	const TArray<UObject*>& ImportedObjects = ImportTask->GetObjects();
	if (ImportedObjects.Num() == 0)
	{
		bIsSuccessful = false;
		OutMessage = "Could not process task - no objects imported";
		return nullptr;
	}
	
	// Log all imported objects for debugging
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Import returned %d objects:"), ImportedObjects.Num());
	for (int32 i = 0; i < ImportedObjects.Num(); i++)
	{
		if (ImportedObjects[i])
		{
			UE_LOG(LogTemp, Log, TEXT("  [%d] %s (%s)"), i, *ImportedObjects[i]->GetPathName(), *ImportedObjects[i]->GetClass()->GetName());
		}
	}
	
	// Find the primary imported object - prefer SkeletalMesh or StaticMesh
	UObject* ImportedObject = nullptr;
	for (UObject* Obj : ImportedObjects)
	{
		if (Obj)
		{
			// Prefer mesh assets over skeleton/physics assets
			if (Cast<USkeletalMesh>(Obj) || Cast<UStaticMesh>(Obj))
			{
				ImportedObject = Obj;
				break;
			}
		}
	}
	
	// Fallback to first non-null object if no mesh found
	if (!ImportedObject)
	{
		for (UObject* Obj : ImportedObjects)
		{
			if (Obj)
			{
				ImportedObject = Obj;
				break;
			}
		}
	}
	
	if (ImportedObject == nullptr)
	{
		bIsSuccessful = false;
		OutMessage = "Import completed but no valid object found";
		return nullptr;
	}
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Selected primary import object: %s"), *ImportedObject->GetPathName());
	bIsSuccessful = true;
	OutMessage = "Import success";
	return ImportedObject;
}

UAssetImportTask* UBridgeManager::CreateImportTask(FString InSourcePath, FString InDestPath, FString InMeshType,
                                                   FString InSkeletonPath, bool& bIsSuccessful, FString& OutMessage)
{
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: === CreateImportTask (glTF) ==="));
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

	// glTF import uses Interchange framework automatically via AssetTools
	// No factory configuration needed - Unreal detects glTF/GLB and uses Interchange
	// The import settings are configured via project settings or Interchange pipelines
	
	const bool bIsSkeletalMesh = InMeshType.Equals(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase);
	
	if (bIsSkeletalMesh)
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: SkeletonPath from JSON: %s"), *InSkeletonPath);
		
		// Check if destination mesh exists
		FString FullAssetPath = InDestPath + TEXT(".") + FPaths::GetBaseFilename(InDestPath);
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Looking for existing mesh at: %s"), *FullAssetPath);
		USkeletalMesh* ExistingMesh = LoadObject<USkeletalMesh>(nullptr, *FullAssetPath);
		if (ExistingMesh)
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Found existing mesh: %s"), *ExistingMesh->GetPathName());
			if (ExistingMesh->GetSkeleton())
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Existing mesh skeleton: %s"), *ExistingMesh->GetSkeleton()->GetPathName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: No existing mesh found - new import"));
		}
		
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Skeletal mesh import via glTF/Interchange"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Static mesh import via glTF/Interchange"));
	}
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Import task configured:"));
	UE_LOG(LogTemp, Log, TEXT("  - Source: %s"), *ResTask->Filename);
	UE_LOG(LogTemp, Log, TEXT("  - DestPath: %s"), *ResTask->DestinationPath);
	UE_LOG(LogTemp, Log, TEXT("  - DestName: %s"), *ResTask->DestinationName);
	UE_LOG(LogTemp, Log, TEXT("  - bReplaceExisting: %s"), ResTask->bReplaceExisting ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("  - bAutomated: %s"), ResTask->bAutomated ? TEXT("true") : TEXT("false"));

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

void UBridgeManager::FindGeneratedAssetsNearMesh(USkeletalMesh* InMesh, USkeleton*& OutGeneratedSkeleton, UPhysicsAsset*& OutGeneratedPhysicsAsset)
{
	OutGeneratedSkeleton = nullptr;
	OutGeneratedPhysicsAsset = nullptr;
	
	if (!InMesh)
	{
		return;
	}
	
	// Get the mesh's current skeleton and physics asset (these are likely auto-generated)
	OutGeneratedSkeleton = InMesh->GetSkeleton();
	OutGeneratedPhysicsAsset = InMesh->GetPhysicsAsset();
	
	// Log what we found
	if (OutGeneratedSkeleton)
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Found skeleton on mesh: %s"), *OutGeneratedSkeleton->GetPathName());
	}
	if (OutGeneratedPhysicsAsset)
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Found physics asset on mesh: %s"), *OutGeneratedPhysicsAsset->GetPathName());
	}
}

FSkeletonImportResult UBridgeManager::AnalyzeSkeletalMeshImport(USkeletalMesh* InImportedMesh, const FString& InIntendedSkeletonPath)
{
	FSkeletonImportResult Result;
	Result.ImportedMesh = InImportedMesh;
	Result.IntendedSkeletonPath = InIntendedSkeletonPath;
	
	if (!InImportedMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: AnalyzeSkeletalMeshImport called with null mesh"));
		return Result;
	}
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: === Analyzing Skeletal Mesh Import ==="));
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Mesh: %s"), *InImportedMesh->GetPathName());
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Intended skeleton: %s"), *InIntendedSkeletonPath);
	
	// Find what was generated with the import
	USkeleton* GeneratedSkeleton = nullptr;
	UPhysicsAsset* GeneratedPhysicsAsset = nullptr;
	FindGeneratedAssetsNearMesh(InImportedMesh, GeneratedSkeleton, GeneratedPhysicsAsset);
	
	// Check if intended skeleton exists and is different from what was generated
	USkeleton* IntendedSkeleton = nullptr;
	if (!InIntendedSkeletonPath.IsEmpty())
	{
		IntendedSkeleton = LoadObject<USkeleton>(nullptr, *InIntendedSkeletonPath);
		if (IntendedSkeleton)
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Found intended skeleton: %s"), *IntendedSkeleton->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Could not load intended skeleton at: %s"), *InIntendedSkeletonPath);
		}
	}
	
	// Determine if a new skeleton was generated (different from intended)
	if (GeneratedSkeleton)
	{
		Result.GeneratedSkeletonPath = GeneratedSkeleton->GetPathName();
		
		if (IntendedSkeleton)
		{
			// Check if the generated skeleton is different from the intended one
			if (GeneratedSkeleton != IntendedSkeleton)
			{
				Result.bNewSkeletonGenerated = true;
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: New skeleton was auto-generated (different from intended)"));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Mesh is using the intended skeleton - no retargeting needed"));
			}
		}
		else if (!InIntendedSkeletonPath.IsEmpty())
		{
			// Intended skeleton was specified but couldn't be loaded - assume new was generated
			Result.bNewSkeletonGenerated = true;
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: New skeleton generated (intended skeleton not found)"));
		}
	}
	
	// Check for auto-generated physics asset
	if (GeneratedPhysicsAsset)
	{
		Result.GeneratedPhysicsAssetPath = GeneratedPhysicsAsset->GetPathName();
		
		// Physics assets are typically auto-generated if they're in the same package/folder as the mesh
		FString MeshPath = FPaths::GetPath(InImportedMesh->GetPathName());
		FString PhysicsPath = FPaths::GetPath(GeneratedPhysicsAsset->GetPathName());
		
		// If physics asset is in same location or a subfolder, it was likely auto-generated
		if (PhysicsPath.Contains(MeshPath) || MeshPath.Contains(PhysicsPath))
		{
			Result.bNewPhysicsAssetGenerated = true;
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Physics asset appears to be auto-generated"));
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Analysis complete - NewSkeleton: %s, NewPhysicsAsset: %s"),
		Result.bNewSkeletonGenerated ? TEXT("Yes") : TEXT("No"),
		Result.bNewPhysicsAssetGenerated ? TEXT("Yes") : TEXT("No"));
	
	return Result;
}

bool UBridgeManager::PromptUserForSkeletonRetarget(const FSkeletonImportResult& InImportResult)
{
	if (!InImportResult.bNewSkeletonGenerated)
	{
		return false;
	}
	
	// Build the message
	FString Message = FString::Printf(
		TEXT("The imported skeletal mesh has a new auto-generated skeleton.\n\n")
		TEXT("Mesh: %s\n")
		TEXT("Generated Skeleton: %s\n")
		TEXT("Intended Skeleton: %s\n\n")
		TEXT("Would you like to retarget this mesh to use the intended skeleton?\n")
		TEXT("This will also delete the auto-generated skeleton and physics asset."),
		InImportResult.ImportedMesh ? *InImportResult.ImportedMesh->GetName() : TEXT("Unknown"),
		*InImportResult.GeneratedSkeletonPath,
		*InImportResult.IntendedSkeletonPath
	);
	
	EAppReturnType::Type UserChoice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::FromString(Message),
		FText::FromString(TEXT("Skeleton Retargeting"))
	);
	
	return UserChoice == EAppReturnType::Yes;
}

void UBridgeManager::RetargetSkeletalMeshToSkeleton(const FSkeletonImportResult& InImportResult, bool bDeleteGeneratedAssets, bool& bIsSuccessful, FString& OutMessage)
{
	bIsSuccessful = false;
	
	if (!InImportResult.ImportedMesh)
	{
		OutMessage = TEXT("No imported mesh to retarget");
		return;
	}
	
	if (InImportResult.IntendedSkeletonPath.IsEmpty())
	{
		OutMessage = TEXT("No intended skeleton path specified");
		return;
	}
	
	// Load the intended skeleton
	USkeleton* IntendedSkeleton = LoadObject<USkeleton>(nullptr, *InImportResult.IntendedSkeletonPath);
	if (!IntendedSkeleton)
	{
		OutMessage = FString::Printf(TEXT("Could not load intended skeleton: %s"), *InImportResult.IntendedSkeletonPath);
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: === Retargeting Skeletal Mesh ==="));
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Mesh: %s"), *InImportResult.ImportedMesh->GetPathName());
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Target Skeleton: %s"), *IntendedSkeleton->GetPathName());
	
	USkeletalMesh* Mesh = InImportResult.ImportedMesh;
	
	// Store references to generated assets before reassignment
	USkeleton* OldSkeleton = Mesh->GetSkeleton();
	UPhysicsAsset* OldPhysicsAsset = Mesh->GetPhysicsAsset();
	
	// Check skeleton compatibility by comparing bone structure
	const FReferenceSkeleton& MeshRefSkeleton = Mesh->GetRefSkeleton();
	const FReferenceSkeleton& TargetRefSkeleton = IntendedSkeleton->GetReferenceSkeleton();
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Mesh has %d bones, Target skeleton has %d bones"),
		MeshRefSkeleton.GetNum(), TargetRefSkeleton.GetNum());
	
	// Verify bone compatibility - check that all mesh bones exist in target skeleton
	bool bBonesCompatible = true;
	for (int32 BoneIdx = 0; BoneIdx < MeshRefSkeleton.GetNum(); ++BoneIdx)
	{
		FName BoneName = MeshRefSkeleton.GetBoneName(BoneIdx);
		int32 TargetBoneIdx = TargetRefSkeleton.FindBoneIndex(BoneName);
		
		if (TargetBoneIdx == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Bone '%s' not found in target skeleton"), *BoneName.ToString());
			bBonesCompatible = false;
		}
	}
	
	if (!bBonesCompatible)
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Some bones are missing in target skeleton - will merge them"));
	}
	
	// IMPORTANT: Order of operations for skeleton reassignment:
	// 1. First merge the mesh's bones into the target skeleton
	// 2. Mark skeleton dirty so merged bones persist
	// 3. Then set the skeleton on the mesh
	// 4. Rebuild mesh LOD info to use new skeleton
	
	// Step 1: Merge bones from mesh into target skeleton FIRST
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Merging mesh bones into target skeleton..."));
	IntendedSkeleton->MergeAllBonesToBoneTree(Mesh);
	IntendedSkeleton->MarkPackageDirty();
	
	// Step 2: Set the skeleton on the mesh
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Setting skeleton on mesh..."));
	Mesh->SetSkeleton(IntendedSkeleton);
	
	// Step 3: Rebuild the mesh's ref skeleton to match the new skeleton
	// This is critical - we need to rebuild the LOD info with correct bone indices
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Rebuilding mesh reference skeleton..."));
	
	// Get the mesh's ref skeleton and rebuild it from the target skeleton
	FReferenceSkeleton& MeshRefSkel = const_cast<FReferenceSkeleton&>(Mesh->GetRefSkeleton());
	
	// Create bone map from mesh bones to skeleton bones
	TArray<int32> BoneMap;
	BoneMap.SetNum(MeshRefSkel.GetNum());
	
	const FReferenceSkeleton& SkeletonRefSkel = IntendedSkeleton->GetReferenceSkeleton();
	for (int32 BoneIdx = 0; BoneIdx < MeshRefSkel.GetNum(); ++BoneIdx)
	{
		FName BoneName = MeshRefSkel.GetBoneName(BoneIdx);
		int32 SkeletonBoneIdx = SkeletonRefSkel.FindBoneIndex(BoneName);
		BoneMap[BoneIdx] = SkeletonBoneIdx;
		
		if (SkeletonBoneIdx != INDEX_NONE)
		{
			UE_LOG(LogTemp, Verbose, TEXT("AssetsBridge: Bone '%s' mapped: mesh[%d] -> skeleton[%d]"), 
				*BoneName.ToString(), BoneIdx, SkeletonBoneIdx);
		}
	}
	
	// Clear the physics asset (user may want to assign a shared one or regenerate)
	Mesh->SetPhysicsAsset(nullptr);
	
	// Mark the mesh as dirty
	Mesh->MarkPackageDirty();
	
	// Force a PostEditChange to rebuild all internal mesh data
	Mesh->PostEditChange();
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Skeleton reassigned successfully"));
	
	// Delete auto-generated assets if requested
	if (bDeleteGeneratedAssets)
	{
		// Delete the old skeleton ONLY if it was auto-generated during this import and different from intended
		if (OldSkeleton && OldSkeleton != IntendedSkeleton && InImportResult.bNewSkeletonGenerated)
		{
			FString OldSkeletonPath = OldSkeleton->GetPathName();
			
			// Additional safety check: only delete if the path matches the generated path
			if (OldSkeletonPath == InImportResult.GeneratedSkeletonPath)
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Deleting auto-generated skeleton: %s"), *OldSkeletonPath);
				
				// Close any editors using this asset
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(OldSkeleton);
				
				// Delete the asset
				bool bDeleted = UEditorAssetLibrary::DeleteAsset(OldSkeletonPath);
				if (bDeleted)
				{
					UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Successfully deleted auto-generated skeleton"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Failed to delete auto-generated skeleton"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Preserving skeleton (path mismatch - may be pre-existing): %s"), *OldSkeletonPath);
			}
		}
		else if (OldSkeleton && OldSkeleton != IntendedSkeleton)
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Preserving pre-existing skeleton: %s"), *OldSkeleton->GetPathName());
		}
		
		// Delete the old physics asset ONLY if it was auto-generated during this import
		// Pre-existing physics assets should be preserved
		if (OldPhysicsAsset && InImportResult.bNewPhysicsAssetGenerated)
		{
			FString OldPhysicsPath = OldPhysicsAsset->GetPathName();
			
			// Additional safety check: only delete if the path matches the generated path
			if (OldPhysicsPath == InImportResult.GeneratedPhysicsAssetPath)
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Deleting auto-generated physics asset: %s"), *OldPhysicsPath);
				
				// Close any editors using this asset
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(OldPhysicsAsset);
				
				// Delete the asset
				bool bDeleted = UEditorAssetLibrary::DeleteAsset(OldPhysicsPath);
				if (bDeleted)
				{
					UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Successfully deleted auto-generated physics asset"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AssetsBridge: Failed to delete auto-generated physics asset"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Preserving physics asset (path mismatch - may be pre-existing): %s"), *OldPhysicsPath);
			}
		}
		else if (OldPhysicsAsset)
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Preserving pre-existing physics asset: %s"), *OldPhysicsAsset->GetPathName());
		}
	}
	
	bIsSuccessful = true;
	OutMessage = FString::Printf(TEXT("Successfully retargeted mesh to skeleton: %s"), *IntendedSkeleton->GetName());
	
	// Show notification to user
	UAssetsBridgeTools::ShowNotification(OutMessage);
}

UObject* UBridgeManager::RelocateImportedAsset(UObject* InImportedAsset, const FString& InIntendedPath, bool& bIsSuccessful, FString& OutMessage)
{
	if (!InImportedAsset)
	{
		bIsSuccessful = false;
		OutMessage = TEXT("No asset to relocate");
		return nullptr;
	}
	
	// Get current asset path
	FString CurrentPath = InImportedAsset->GetPathName();
	FString CurrentPackagePath = InImportedAsset->GetOutermost()->GetPathName();
	FString AssetName = InImportedAsset->GetName();
	
	// Build intended full path (package path + asset name)
	FString IntendedPackagePath = InIntendedPath;
	FString IntendedFullPath = IntendedPackagePath + TEXT(".") + AssetName;
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Checking if relocation needed"));
	UE_LOG(LogTemp, Log, TEXT("  Current: %s"), *CurrentPath);
	UE_LOG(LogTemp, Log, TEXT("  Intended: %s"), *IntendedFullPath);
	
	// Check if already at correct location
	if (CurrentPackagePath == IntendedPackagePath)
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Asset already at correct location"));
		bIsSuccessful = true;
		OutMessage = TEXT("Asset already at correct location");
		return InImportedAsset;
	}
	
	// Store original folder for cleanup later
	FString OriginalFolder = FPaths::GetPath(CurrentPackagePath);
	
	// Check if destination already exists
	if (UEditorAssetLibrary::DoesAssetExist(IntendedPackagePath))
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Destination already exists, deleting old asset first"));
		
		// Load and close editors for existing asset
		UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(IntendedPackagePath);
		if (ExistingAsset)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(ExistingAsset);
		}
		
		// Delete existing asset
		if (!UEditorAssetLibrary::DeleteAsset(IntendedPackagePath))
		{
			bIsSuccessful = false;
			OutMessage = FString::Printf(TEXT("Failed to delete existing asset at: %s"), *IntendedPackagePath);
			return InImportedAsset;
		}
	}
	
	// Rename/move the asset to the intended location
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Relocating asset from %s to %s"), *CurrentPackagePath, *IntendedPackagePath);
	
	if (UEditorAssetLibrary::RenameAsset(CurrentPackagePath, IntendedPackagePath))
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Asset relocated successfully"));
		
		// Clean up empty folders
		CleanupEmptyInterchangeFolders(OriginalFolder);
		
		// Load the relocated asset
		UObject* RelocatedAsset = UEditorAssetLibrary::LoadAsset(IntendedPackagePath);
		if (RelocatedAsset)
		{
			bIsSuccessful = true;
			OutMessage = FString::Printf(TEXT("Asset relocated to: %s"), *IntendedPackagePath);
			return RelocatedAsset;
		}
		else
		{
			bIsSuccessful = false;
			OutMessage = TEXT("Asset relocated but failed to reload");
			return nullptr;
		}
	}
	else
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("Failed to relocate asset from %s to %s"), *CurrentPackagePath, *IntendedPackagePath);
		return InImportedAsset;
	}
}

void UBridgeManager::CleanupEmptyInterchangeFolders(const FString& InFolderPath)
{
	if (InFolderPath.IsEmpty())
	{
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Checking folder for cleanup: %s"), *InFolderPath);
	
	// Get all assets in this folder (non-recursive)
	TArray<FString> AssetsInFolder = UEditorAssetLibrary::ListAssets(InFolderPath, false, false);
	
	if (AssetsInFolder.Num() == 0)
	{
		// Folder is empty, try to delete it
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Folder is empty, attempting to delete: %s"), *InFolderPath);
		
		if (UEditorAssetLibrary::DeleteDirectory(InFolderPath))
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Successfully deleted empty folder: %s"), *InFolderPath);
			
			// Recursively check parent folder
			FString ParentFolder = FPaths::GetPath(InFolderPath);
			if (!ParentFolder.IsEmpty() && ParentFolder != TEXT("/Game"))
			{
				CleanupEmptyInterchangeFolders(ParentFolder);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Could not delete folder (may have subfolders): %s"), *InFolderPath);
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Folder not empty (%d assets), skipping: %s"), AssetsInFolder.Num(), *InFolderPath);
	}
}

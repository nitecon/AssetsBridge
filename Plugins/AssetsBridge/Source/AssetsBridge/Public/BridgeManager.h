// Copyright 2023 Nitecon Studios LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BridgeManager.generated.h"

class UAssetImportTask;
class UFactory;


/**
 * 
 */
UCLASS()
class ASSETSBRIDGE_API UBridgeManager : public UObject
{
public:
	GENERATED_BODY()

	/** Generic constructor */
	UBridgeManager();

	/**
	 * This function is responsible for creating the export bundle that will be saved and made available for external 3D application.
	 * 
	 * @param SelectList the array of items currently selected within the level
	 * @param ContentList the array of items currently selected within the content browser.
	 * @param bIsSuccessful indicates whether operation was successful
	 * @param OutMessage provides verbose information on the status of the operation.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Exports")
	static void ExecuteSwap(TArray<AActor*> SelectList, TArray<FAssetData> ContentList, bool& bIsSuccessful,
	                        FString& OutMessage);

	/**
	 * DEPRECATED: This function is no longer being used.
	 * This functions checks to see if the actor path is part of "Engine" content, so it can be duplicated first.
	 * @param Path This is the path that is to be evaluated whether it is within system directories.
	 * @return Returns true if it is deemed to be an engine item false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Tools")
	static bool IsSystemPath(FString Path);


	/**
	 * This function is responsible for duplicating the engine selected items and swapping them in the level with the new items.
	 * @param InAsset uses the engine information to create a duplicate and switch it out.
	 * @param bIsSuccessful indicates whether operation was successful
	 * @param OutMessage provides verbose information on the status of the operation.
	 * @return Returns the updated FExportAsset with the new duplicated path.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Tools")
	static FExportAsset DuplicateAndSwap(FExportAsset InAsset, bool& bIsSuccessful, FString& OutMessage);


	/**
	 * This function is responsible for checking to see if we already have an existing export item that exists..
	 * 
	 * @param Assets the array of existing export items.
	 * @param InAsset the asset to validate if it already exists.
	 * @returns Whether the assets array contains the same item at it's respective path.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Exports")
	static bool HasMatchingExport(TArray<FExportAsset> Assets, FAssetData InAsset);


	/**
	 * DEPRECATED: This function is no longer being used.
	 * This generates a checksum from the FTransform object.
	 * @param Object the FTransform object to generate a checksum from.
	 * @return Returns a string representation of the checksum.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Tools")
	static FString ComputeTransformChecksum(FWorldData& Object);


	/**
	 * This function is responsible for checking to see if we have something selected duplicating engine content and assigning the array to GenerateExport.
	 * 
	 * @param bIsSuccessful indicates whether operation was successful
	 * @param OutMessage provides verbose information on the status of the operation.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Exports")
	static void StartExport(bool& bIsSuccessful, FString& OutMessage);


	/**
	 * This function is responsible for creating the export bundle that will be saved and made available for external 3D application.
	 * 
	 * @param AssetList contains the array of all assets to be exported to the scene.
	 * @param bIsSuccessful indicates whether operation was successful
	 * @param OutMessage provides verbose information on the status of the operation.
	 */
	static void GenerateExport(TArray<FExportAsset> AssetList, bool& bIsSuccessful, FString& OutMessage);

	/**
	 * This function is responsible for reading the manifest and importing the associated mesh in level or multiple meshes to asset library.
	 * 
	 * @param bIsSuccessful indicates whether operation was successful
	 * @param OutMessage provides verbose information on the status of the operation.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Exports")
	static void GenerateImport(bool& bIsSuccessful, FString& OutMessage);

	/**
	 * This function provides a means to replace the current references of an old packages to reference the new package instead.
	 */
	UFUNCTION()
	static void ReplaceRefs(FString OldPackageName, UPackage* NewPackage, bool& bIsSuccessful, FString& OutMessage);

	/*
	 * Checks to see if a package exists at a particular path
	 */
	static bool HasExistingPackageAtPath(FString InPath);

	UFUNCTION(BlueprintCallable, Category="Asset Bridge Tools")
	static UObject* ImportAsset(FString InSourcePath, FString InDestPath, FString InMeshType, FString InSkeletonPath, bool& bIsSuccessful, FString& OutMessage);

private:
	static UObject* ProcessTask(UAssetImportTask* ImportTask, bool& bIsSuccessful, FString& OutMessage);
	static UAssetImportTask* CreateImportTask(FString InSourcePath, FString InDestPath, FString InMeshType,
	                                          FString InSkeletonPath, bool& bIsSuccessful, FString& OutMessage);
	static void ExportObject(FString InObjInternalPath, FString InDestPath, bool& bIsSuccessful, FString& OutMessage);
};

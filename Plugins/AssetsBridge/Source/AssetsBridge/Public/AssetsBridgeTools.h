// Copyright 2023 Nitecon Studios LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetsBridgeTools.generated.h"


USTRUCT(BlueprintType)
struct FMaterialSlot
{
	GENERATED_BODY()

	/** Name of the material / slot name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Material")
	FString Name = "";

	/** Material index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Material")
	int Idx = 0;

	/** Where to find it in the content library. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Material")
	FString InternalPath = "";

	/** Original index before any changes (for tracking removals) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Material")
	int OriginalIdx = -1;
};

USTRUCT(BlueprintType)
struct FMaterialChangeset
{
	GENERATED_BODY()

	/** Materials that were added in Blender (new slots) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Material")
	TArray<FMaterialSlot> Added;

	/** Materials that were removed in Blender */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Material")
	TArray<FMaterialSlot> Removed;

	/** Materials that remain unchanged (restore original Unreal materials) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Material")
	TArray<FMaterialSlot> Unchanged;
};

USTRUCT(BlueprintType)
struct FWorldData
{
	GENERATED_BODY()

	/** mesh pointer for it will be set here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Placement")
	FVector Rotation = FVector::ZeroVector;

	/** mesh pointer for it will be set here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Placement")
	FVector Location = FVector::ZeroVector;

	/** mesh pointer for it will be set here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Placement")
	FVector Scale = FVector::OneVector;

	void Serialize(FArchive& Archive)
	{
		Archive << Rotation;
		Archive << Location;
		Archive << Scale;
	}
};

USTRUCT(BlueprintType)
struct FExportAsset
{
	GENERATED_BODY()

	/** mesh pointer for it will be set here (runtime only, not serialized to JSON). */
	UPROPERTY(Transient, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	UObject* ModelPtr = nullptr;

	/** Full path to the model asset - matches JSON 'model' field for serialization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString Model = "";

	/** unique object identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString ObjectID = "";

	/** Material information for the object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	TArray<FMaterialSlot> ObjectMaterials;

	/** Material changeset tracking additions/removals between Blender and Unreal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FMaterialChangeset MaterialChangeset;

	/** Where to find it in the content library. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString InternalPath = "";

	/** Name of the actual file for use in export. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString RelativeExportPath = "";

	/** Name of the actual file for use in export. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString ShortName = "";

	/** Location of where to export. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString ExportLocation = "";

	/** Mesh type: StaticMesh or SkeletalMesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString StringType = "StaticMesh";

	/** Path to skeleton asset (for skeletal meshes only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FString Skeleton = "";

	/** Morph target names (for skeletal meshes with blend shapes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	TArray<FString> MorphTargets;

	/** World transform data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Details")
	FWorldData WorldData = FWorldData();
};

USTRUCT(BlueprintType, Category="Assets Bridge|JSON")
struct FBridgeExport
{
	GENERATED_BODY()

public:
	/** Name of the actual file for use in export. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|JSON")
	FString Operation = "UnrealExport";

	/** Where to find it in the content library. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|JSON")
	TArray<FExportAsset> Objects;
};

USTRUCT(BlueprintType)
struct FAssetDetails
{
	GENERATED_BODY()

public:
	/** The actor that is currently selected in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Asset Details")
	TWeakObjectPtr<UObject> WorldObject;

	/** This is the asset for the selected item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Asset Details")
	FAssetData ObjectAsset;
};


class UStruct;
/**
 * 
 */
UCLASS()
class ASSETSBRIDGE_API UAssetsBridgeTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates a dialog to inform the user.
	 * @param Message is the message to be displayed in the dialog for the user to read.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static void ShowInfoDialog(FString Message);

	/**
	 * Creates a notification to inform the user.
	 * @param Message is the message to be displayed in the dialog for the user to read.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static void ShowNotification(FString Message);
	/**
	 * Combines name and new internal path and stitches it onto the user provided export base.
	 *
	 * @param NewInternalPath The new location where the asset will be residing.
	 * @param NewName The new name for the asset as provided by the user.
	 * @return Returns the full export path.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static FString GetExportPathFromInternal(FString NewInternalPath, FString NewName);

	/**
	 * Reads a JSON file into a FBridgeExportElement Structure and returns the content as a string.
	 *
	 * @param bIsSuccessful Provides boolean whether operation succeeded.
	 * @param OutMessage Provides more verbose information on the operation.
	 *
	 * @return Returns a list of Bridge Export Elements which are read from a file..
	 */
	UFUNCTION(BlueprintCallable, Category="JSON")
	static FBridgeExport ReadBridgeExportFile(bool& bIsSuccessful, FString& OutMessage);

	/**
		 * Writes a JSON file from a Array of FBridgeExportElement Structure.
		 *
		 * @param Data Contains the data that is to be converted over.
		 * @param bIsSuccessful Provides boolean whether operation succeeded.
		 * @param OutMessage Provides more verbose information on the operation.
		 */
	UFUNCTION(BlueprintCallable, Category="JSON")
	static void WriteBridgeExportFile(FBridgeExport Data, bool& bIsSuccessful, FString& OutMessage);


	/**
	 * This is a utility function to find the currently selected item(s) and select them in the content browser.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static bool ContentBrowserFromWorldSelection();

	/**
	 * Finds the currently selected folder within the content tree / content browser view.
	 *
	 * @param OutContentLocation Sets the currently selected folder path to this value.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static void GetSelectedContentBrowserPath(FString& OutContentLocation);

	/**
	 * This is a utility function to select the current item in the content browser by path on behalf of the user.
	 * @param Assets is the list of items that should be selected in the content browser.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static void SetSelectedContentBrowserItems(TArray<FAssetData> Assets);
	/**
	 * This is a utility function to select the current item in the content browser by path on behalf of the user.
	 * NOTE: this must be the full path, and must include the extension like: /Engine/Cone.Cone not just /Engine/Cone
	 * @param Paths is the path which should be selected for that particular item.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static void SetSelectedContentBrowserPaths(TArray<FString> Paths);

	/**
	 * Finds the currently selected items within the content tree / content browser view.
	 *
	 * @param SelectedAssets Appends the array with the currently selected items.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static void GetSelectedContentBrowserItems(TArray<FAssetData>& SelectedAssets);

	/**
	 * Opens a dialog for the user to browse and select a directory on disk.
	 *
	 * @param DialogTitle Title to add to the dialog box to establish intent for the dialog box.
	 *
	 * @return Provides an FString to be used which contains the location of the Directory.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static FString GetOSDirectoryLocation(const FString& DialogTitle);

	/**
	 * Opens a dialog for the user to browse and select a file on disk.
	 *
	 * @param DialogTitle Title to add to the dialog box to establish intent for the dialog box.
	 * @param FileTypes List of files to filter on in the format TEXT("JSON files (*.json)|*.json").
	 *
	 * @return Provides an FString to be used which contains the location of the file.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static FString GetOSFileLocation(const FString& DialogTitle,
	                                 const FString& FileTypes = TEXT("JSON files (*.json)|*.json"));

	/**
	 * Reads a file and returns the content as a string.
	 *
	 * @param FilePath Location for the file to be read on disk.
	 * @param bIsSuccessful Provides boolean whether operation succeeded.
	 * @param OutMessage Provides more verbose information on the operation.
	 *
	 * @return Provides an FString to be used which contains the content of the file.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static FString ReadStringFromFile(FString FilePath, bool& bIsSuccessful, FString& OutMessage);

	/**
	 * Sets the Assets Bridge content location where assets will be stored (root assets folder)
	 *
	 * @param FilePath Location for the file to be written on disk.
	 * @param Data the contents to be written to the file.
	 * @param bIsSuccessful Provides boolean whether operation succeeded.
	 * @param OutMessage Provides more verbose information on the operation.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static void WriteStringToFile(FString FilePath, FString Data, bool& bIsSuccessful, FString& OutMessage);


	/**
	 * Open a json file read it's content and convert it to a json object
	 *
	 * @param FilePath	Location of the json file on disk.
	 * @param bIsSuccessful Returns true of operation is successful.
	 * @param OutMessage Verbose information on the current operation.
	 *
	 * @return The JsonObject content of your json file.
	 */
	static TSharedPtr<FJsonObject> ReadJson(FString FilePath, bool& bIsSuccessful, FString& OutMessage);

	/**
	* Open a json file read it's content and convert it to a json object
	*
	* @param FilePath	Location of the json file on disk.
	* @param JsonObject Object to write to file
	* @param bIsSuccessful Returns true of operation is successful.
	* @param OutMessage Verbose information on the current operation.
	*/
	static void WriteJson(FString FilePath, TSharedPtr<FJsonObject> JsonObject, bool& bIsSuccessful,
	                      FString& OutMessage);

	/**
	 * Returns the user selected list of items to be exported or interacted with, returns only items that are static meshes or skeletal meshes.
	 */
	UFUNCTION(BlueprintCallable, Category="AssetsBridge Utilities")
	static TArray<AActor*> GetWorldSelection();

	/**
	* Gets the Assets Bridge location related to this setting.
	*
	* @param OutContentLocation Is set to the location for this setting to be consumed by blueprints etc.
	*/
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Settings")
	static void GetExportRoot(FString& OutContentLocation);

	/**
	* Sets the Assets Bridge config option for the related setting.
	*
	* @param InLocation Sets the provided string to the value of the content location.
	*/
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Settings")
	static void SetExportRoot(FString InLocation);

	/**
	* Opens a folder browser dialog to select the export root location.
	* If a folder is selected, it is saved as the new export root.
	*
	* @return The selected folder path, or empty string if cancelled.
	*/
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Settings")
	static FString BrowseForExportRoot();

	/**
	 * Get the asset data for a specific path from the asset manager.
	 * @param Path is a FString path to the object for which we want to retrieve information
	 * @returns FAssetData for the object
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static FAssetData GetAssetDataFromPath(FString Path);

	/**
	 * Utility function to remove the extension from a path's filename
	 */
	static FString GetPathWithoutExt(FString InPath);

	/**
	 * This functions is responsible for stripping engine specific paths so they can be prepended by the asset directory..
	 * @param Path This is the path that is to be evaluated whether it is within system directories.
	 * @return the path in relative ot the asset content path.
	 */
	static FString GetSystemPathAsAssetPath(FString Path);

	/**
	 * Get the asset data for a specific paths from the asset manager.
	 * @param Path is a FString path to the object for which we want to retrieve information
	 * @returns FAssetData for the object
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static TArray<FAssetData> GetAssetDataFromPaths(TArray<FString> Paths);

	/**
	 * Utility function to retrieve the associated asset data from an actor that may exist in the level.
	 * @param InActor is the actor to retrieve the asset data for.
	 * @returns AssetData for the particular actor.
	 */
	UFUNCTION()
	static TArray<FAssetData> GetAssetsFromActor(const AActor* InActor);

	/**
	* Gets additional information from a specific actor which will be used in the import / export pipeline.
	* @returns the list of currently selected items in the world as Assets.
	*/
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static TArray<FAssetDetails> GetWorldSelectedAssets();

	/**
	 * Gets additional information from a specific actor which will be used in the import / export pipeline.
	 * @param AssetInfo is the actor that should be converted to ExportAsset structure.
	 * @param bIsSuccessful Returns true of operation is successful.
	 * @param OutMessage Verbose information on the current operation.
	 */
	UFUNCTION(BlueprintCallable, Category="Assets Bridge Utilities")
	static FExportAsset GetExportInfo(FAssetData AssetInfo, bool& bIsSuccessful, FString& OutMessage);

	template <typename T>
	static FString EnumToString(const FString& enumName, const T value)
	{
		// ANY_PACKAGE deprecated in UE5 - use FindFirstObject instead
		UEnum* pEnum = FindFirstObject<UEnum>(*enumName, EFindFirstObjectOptions::NativeFirst);
		return *(pEnum ? pEnum->GetNameStringByIndex(static_cast<uint8>(value)) : "null");
	}
};

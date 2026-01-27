// Copyright 2023 Nitecon Studios LLC. All rights reserved.

#include "AssetsBridgeTools.h"

#include "ABSettings.h"
#include "ContentBrowserModule.h"
#include "EditorDirectories.h"
#include "IContentBrowserSingleton.h"
#include "Selection.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Engine/AssetManager.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Animation/MorphTarget.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Notifications/SNotificationList.h"

void UAssetsBridgeTools::ShowInfoDialog(FString Message)
{
	FText DialogText = FText::FromString(Message);
	FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

void UAssetsBridgeTools::ShowNotification(FString Message)
{
	FSlateNotificationManager::Get().AddNotification(FNotificationInfo(FText::FromString(Message)));
}

FString UAssetsBridgeTools::GetExportPathFromInternal(FString NewInternalPath, FString NewName)
{
	FString AssetHome;
	GetExportRoot(AssetHome);
	// TODO: Strip Engine / Game / Other folders from the start.
	FString NewExportPath = FPaths::Combine(AssetHome, NewInternalPath, NewName.Append(".glb"));
	UE_LOG(LogTemp, Warning, TEXT("Adding new export path: %s"), *NewExportPath)
	return NewExportPath;
}

FBridgeExport UAssetsBridgeTools::ReadBridgeExportFile(bool& bIsSuccessful, FString& OutMessage)
{
	FString AssetBase;
	GetExportRoot(AssetBase);
	// Read from Blender's export file (bidirectional: Blender writes from-blender.json, Unreal reads it)
	FString JsonFilePath = FPaths::Combine(AssetBase, "from-blender.json");
	
	// Fallback to legacy file if new format doesn't exist
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*JsonFilePath))
	{
		FString LegacyPath = FPaths::Combine(AssetBase, "AssetBridge.json");
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*LegacyPath))
		{
			JsonFilePath = LegacyPath;
			UE_LOG(LogTemp, Warning, TEXT("Using legacy AssetBridge.json - consider updating Blender addon to use from-blender.json"));
		}
	}
	
	// Try to read generic text into json object
	TSharedPtr<FJsonObject> JSONObject = ReadJson(JsonFilePath, bIsSuccessful, OutMessage);
	{
		if (!bIsSuccessful)
		{
			return FBridgeExport();
		}
	}
	FBridgeExport ReturnData;
	if (!FJsonObjectConverter::JsonObjectToUStruct<FBridgeExport>(JSONObject.ToSharedRef(), &ReturnData))
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("Invalid json detected for this operation on file: %s"), *JsonFilePath);
		return FBridgeExport();
	}
	bIsSuccessful = true;
	OutMessage = FString::Printf(TEXT("Read %d objects from %s"), ReturnData.Objects.Num(), *JsonFilePath);
	return ReturnData;
}

void UAssetsBridgeTools::WriteBridgeExportFile(FBridgeExport Data, bool& bIsSuccessful, FString& OutMessage)
{
	TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(Data);
	if (JsonObject == nullptr)
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("Invalid struct received, cannot convert to json"));
		return;
	}
	// Write to Unreal's export file (bidirectional: Unreal writes from-unreal.json, Blender reads it)
	FString BridgeName = "from-unreal.json";
	FString AssetBase;
	GetExportRoot(AssetBase);
	FString JsonFilePath = FPaths::Combine(AssetBase, BridgeName);
	WriteJson(JsonFilePath, JsonObject, bIsSuccessful, OutMessage);
	
	if (bIsSuccessful)
	{
		OutMessage = FString::Printf(TEXT("Exported %d objects to %s"), Data.Objects.Num(), *JsonFilePath);
	}
}

bool UAssetsBridgeTools::ContentBrowserFromWorldSelection()
{
	TArray<AActor*> Selection = GetWorldSelection();
	if (Selection.Num() < 1)
	{
		return false;
	}
	TArray<FString> SelectedPaths;
	for (AActor* Actor : Selection)
	{
		FAssetData ItemData = GetAssetDataFromPath(Actor->GetPathName());
		if (ItemData.IsValid())
		{
			SelectedPaths.Add(ItemData.PackagePath.ToString());
		}
	}
	if (SelectedPaths.Num() < 1)
	{
		return false;
	}
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	IContentBrowserSingleton& ContentBrowserSingleton = ContentBrowserModule.Get();
	ContentBrowserSingleton.SetSelectedPaths(SelectedPaths, true);
	return true;
}

void UAssetsBridgeTools::GetSelectedContentBrowserPath(FString& OutContentLocation)
{
	TArray<FAssetData> OutSelectedAssets;
	TArray<FString> OutSelectedFolders;
	TArray<FString> OutViewFolders;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	IContentBrowserSingleton& ContentBrowserSingleton = ContentBrowserModule.Get();

	ContentBrowserSingleton.GetSelectedFolders(OutSelectedFolders);
	ContentBrowserSingleton.GetSelectedPathViewFolders(OutViewFolders);
	// First select the last view item.
	for (auto Asset : OutViewFolders)
	{
		// UE_LOG(LogTemp, Warning, TEXT("View Folder is: %s"), *Asset)
		//  We do a replace since "show all" in content browser can cause a change in the virtual path
		OutContentLocation = Asset.Replace(TEXT("/All"), TEXT(""));
	}
	// Now we iterate through the non view path and select here.
	for (auto Asset : OutSelectedFolders)
	{
		// UE_LOG(LogTemp, Warning, TEXT("Asset is: %s"), *Asset)
		//  We do a replace since "show all" in content browser can cause a change in the virtual path
		OutContentLocation = Asset.Replace(TEXT("/All"), TEXT(""));
	}
}

void UAssetsBridgeTools::SetSelectedContentBrowserItems(TArray<FAssetData> Assets)
{
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	IContentBrowserSingleton& ContentBrowserSingleton = ContentBrowserModule.Get();
	ContentBrowserSingleton.SyncBrowserToAssets(Assets);
}

void UAssetsBridgeTools::SetSelectedContentBrowserPaths(TArray<FString> Paths)
{
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	IContentBrowserSingleton& ContentBrowserSingleton = ContentBrowserModule.Get();
	TArray<FAssetData> AssetDatas = GetAssetDataFromPaths(Paths);
	ContentBrowserSingleton.SyncBrowserToAssets(AssetDatas);
}

void UAssetsBridgeTools::GetSelectedContentBrowserItems(TArray<FAssetData>& SelectedAssets)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	IContentBrowserSingleton& ContentBrowserSingleton = ContentBrowserModule.Get();
	ContentBrowserSingleton.GetSelectedAssets(SelectedAssets);
}

FString UAssetsBridgeTools::GetOSDirectoryLocation(const FString& DialogTitle)
{
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString DestinationFolder;
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const FString DefaultLocation(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT));

		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			ParentWindowHandle,
			DialogTitle,
			DefaultLocation,
			DestinationFolder);

		if (bFolderSelected)
		{
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, DestinationFolder);
			return FPaths::ConvertRelativePathToFull(DestinationFolder);
		}
	}
	return FString("Unknown");
}

FString UAssetsBridgeTools::GetOSFileLocation(const FString& DialogTitle, const FString& FileTypes)
{
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString DestinationFolder;
		TArray<FString> OutFiles;
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		const FString DefaultLocation(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT));

		const bool bFolderSelected = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			DialogTitle,
			DefaultLocation,
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OutFiles);

		if (bFolderSelected && OutFiles.Num() > 0)
		{
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, DestinationFolder);
			return FPaths::ConvertRelativePathToFull(OutFiles[0]);
		}
	}
	return FString("Unknown");
}

FString UAssetsBridgeTools::ReadStringFromFile(FString FilePath, bool& bIsSuccessful, FString& OutMessage)
{
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("failed to open file for reading: '%s'"), *FilePath);
		return "";
	}

	FString Result = "";
	if (!FFileHelper::LoadFileToString(Result, *FilePath))
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("unable to read file: '%s'"), *FilePath);
		return "";
	}
	bIsSuccessful = true;
	OutMessage = "success";
	return Result;
}

void UAssetsBridgeTools::WriteStringToFile(FString FilePath, FString Data, bool& bIsSuccessful, FString& OutMessage)
{
	if (!FFileHelper::SaveStringToFile(Data, *FilePath))
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("failed to write file: '%s'"), *FilePath);
		return;
	}
	bIsSuccessful = true;
	OutMessage = FString::Printf(TEXT("wrote file: %s"), *FilePath);
}

TSharedPtr<FJsonObject> UAssetsBridgeTools::ReadJson(FString FilePath, bool& bIsSuccessful, FString& OutMessage)
{
	FString StringData = ReadStringFromFile(FilePath, bIsSuccessful, OutMessage);
	if (!bIsSuccessful)
	{
		return nullptr;
	}
	TSharedPtr<FJsonObject> ReturnObj;

	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(StringData), ReturnObj))
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("failed to read json object: %s"), *StringData);
		return nullptr;
	}
	bIsSuccessful = true;
	OutMessage = FString::Printf(TEXT("json read success from %s"), *FilePath);
	return ReturnObj;
}

void UAssetsBridgeTools::WriteJson(FString FilePath, TSharedPtr<FJsonObject> JsonObject, bool& bIsSuccessful,
                                   FString& OutMessage)
{
	FString JsonString;
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&JsonString, 0)))
	{
		bIsSuccessful = false;
		OutMessage = FString::Printf(TEXT("failed to write json file: %s"), *FilePath);
		return;
	}
	WriteStringToFile(FilePath, JsonString, bIsSuccessful, OutMessage);
	if (!bIsSuccessful)
	{
		return;
	}
	bIsSuccessful = true;
	OutMessage = FString::Printf(TEXT("wrote json to file: %s"), *FilePath);
}


TArray<AActor*> UAssetsBridgeTools::GetWorldSelection()
{
	TArray<AActor*> OutActors;
	// TODO: Add filter for static /skeletal meshes only.
	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		TArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		if (Components.Num() > 0)
		{
			OutActors.Add(Actor);
		}
	}
	return OutActors;
}


void UAssetsBridgeTools::GetExportRoot(FString& OutContentLocation)
{
	UABSettings* Settings = GetMutableDefault<UABSettings>();
	if (Settings != nullptr)
	{
		OutContentLocation = Settings->AssetLocationOnDisk;
	}
}

void UAssetsBridgeTools::SetExportRoot(FString InLocation)
{
	UABSettings* Settings = GetMutableDefault<UABSettings>();
	if (Settings != nullptr)
	{
		Settings->AssetLocationOnDisk = InLocation;
		Settings->SaveConfig();
	}
}

FAssetData UAssetsBridgeTools::GetAssetDataFromPath(FString Path)
{
	UAssetManager& AssetManager = UAssetManager::Get();
	const FSoftObjectPath& AssetPath = Path;
	FAssetData AssetData;
	AssetManager.GetAssetDataForPath(AssetPath, AssetData);
	return AssetData;
}

FString UAssetsBridgeTools::GetPathWithoutExt(FString InPath)
{
	FString PackagePath;
	InPath.Split(".", &PackagePath, NULL);
	return PackagePath;
}

FString UAssetsBridgeTools::GetSystemPathAsAssetPath(FString Path)
{
	// Clean up virtual path prefixes, keep as content-relative path
	FString LocalPath = Path.Replace(TEXT("/All"), TEXT("")).Replace(TEXT("/Game"), TEXT(""));
	// Convert to actual disk path under project Content directory
	FString ContentDir = FPaths::ProjectContentDir();
	FString ObjectPath = FPaths::Combine(ContentDir, LocalPath);
	return ObjectPath;
}

TArray<FAssetData> UAssetsBridgeTools::GetAssetDataFromPaths(TArray<FString> Paths)
{
	TArray<FAssetData> Assets;
	for (FString Path : Paths)
	{
		FAssetData Item = GetAssetDataFromPath(Path);
		if (Item.IsValid())
		{
			Assets.Add(Item);
		}
	}
	return Assets;
}

TArray<FAssetData> UAssetsBridgeTools::GetAssetsFromActor(const AActor* InActor)
{
	TArray<FAssetData> Assets;
	if (InActor != nullptr)
	{
		GEditor->SyncToContentBrowser();
		GetSelectedContentBrowserItems(Assets);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Provided actor is null."))
	}
	return Assets;
}

TArray<FAssetDetails> UAssetsBridgeTools::GetWorldSelectedAssets()
{
	TArray<FAssetDetails> Items;
	auto CurSelection = GEditor->GetSelectedActors();
	TArray<TWeakObjectPtr<UObject>> Objects;
	CurSelection->GetSelectedObjects(Objects);
	for (TWeakObjectPtr<UObject> obj : Objects)
	{
		FAssetData Item = GetAssetDataFromPath(obj->GetDetailedInfo());
		if (Item != nullptr)
		{
			FAssetDetails NewItem;
			NewItem.ObjectAsset = Item;
			NewItem.WorldObject = obj;
			Items.Add(NewItem);
		}
	}
	return Items;
}

FExportAsset UAssetsBridgeTools::GetExportInfo(FAssetData AssetInfo, bool& bIsSuccessful, FString& OutMessage)
{
	FExportAsset Result;
	FString AssetPath;
	GetExportRoot(AssetPath);
	FString BasePath;
	FString ShortName;
	FString Discard;
	FPaths::Split(AssetInfo.GetObjectPathString(), BasePath, ShortName, Discard);
	FString RelativeContentPath = BasePath.Replace(TEXT("/Game"), TEXT(""));
	Result.ModelPtr = AssetInfo.GetAsset();
	Result.Model = AssetInfo.GetObjectPathString();  // Store full path for reimport
	Result.ShortName = ShortName;
	FString FileName = ShortName.Append(".glb");
	FString ExportLoc = FPaths::Combine(AssetPath, RelativeContentPath, FileName);
	Result.ExportLocation = ExportLoc;
	Result.InternalPath = RelativeContentPath;

	Result.RelativeExportPath = RelativeContentPath;
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Result.ModelPtr);
	if (StaticMesh != nullptr)
	{
		Result.StringType = "StaticMesh";
		TArray<FStaticMaterial> Materials = StaticMesh->GetStaticMaterials();
		for (int32 Idx = 0; Idx < Materials.Num(); Idx++)
		{
			const FStaticMaterial& Mat = Materials[Idx];
			FMaterialSlot NewSlotMat;
			NewSlotMat.Idx = Idx;
			NewSlotMat.Name = Mat.MaterialSlotName.ToString();
			NewSlotMat.InternalPath = Mat.MaterialInterface ? GetPathWithoutExt(Mat.MaterialInterface.GetPath()) : TEXT("");
			Result.ObjectMaterials.Add(NewSlotMat);
		}
		bIsSuccessful = true;
		OutMessage = FString(TEXT("Data retrieved for static mesh"));
		return Result;
	}
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Result.ModelPtr);
	if (SkeletalMesh != nullptr)
	{
		Result.StringType = "SkeletalMesh";
		// Store skeleton path for reimport
		if (SkeletalMesh->GetSkeleton())
		{
			Result.Skeleton = SkeletalMesh->GetSkeleton()->GetPathName();
		}
		// Capture morph target names for preservation through glTF round-trip
		const TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->GetMorphTargets();
		for (const UMorphTarget* MorphTarget : MorphTargets)
		{
			if (MorphTarget)
			{
				Result.MorphTargets.Add(MorphTarget->GetName());
				UE_LOG(LogTemp, Log, TEXT("AssetsBridge: Captured morph target: %s"), *MorphTarget->GetName());
			}
		}
		TArray<FSkeletalMaterial> Materials = SkeletalMesh->GetMaterials();
		for (int32 Idx = 0; Idx < Materials.Num(); Idx++)
		{
			const FSkeletalMaterial& Mat = Materials[Idx];
			FMaterialSlot NewSlotMat;
			NewSlotMat.Idx = Idx;
			NewSlotMat.Name = Mat.MaterialSlotName.ToString();
			NewSlotMat.InternalPath = Mat.MaterialInterface ? GetPathWithoutExt(Mat.MaterialInterface.GetPath()) : TEXT("");
			Result.ObjectMaterials.Add(NewSlotMat);
		}
		bIsSuccessful = true;
		OutMessage = FString(TEXT("Data retrieved for skeletal mesh"));
		return Result;
	}
	Result.StringType = "Unknown";
	bIsSuccessful = true;
	OutMessage = FString(TEXT("Data retrieved for unknown object"));
	return Result;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetsBridge.generated.h"

class FToolBarBuilder;
class FMenuBuilder;
class AStaticMeshActor;

UENUM(BlueprintType)
enum class EBridgeType : uint8
{
	Unknown = 0 UMETA(DisplayName = "Unsupported"),
	StaticMesh = 1 UMETA(DisplayName = "Static Mesh"),
	SkeletalMesh = 2 UMETA(DisplayName = "Skeletal Mesh"),
	Animation = 3 UMETA(DisplayName = "Animation"),
};

static const TCHAR* EnumToString(EBridgeType InCurrentState)
{
	switch (InCurrentState)
	{
	case EBridgeType::StaticMesh:
		return TEXT("Static Mesh");
	case EBridgeType::SkeletalMesh:
		return TEXT("Skeletal Mesh");
	case EBridgeType::Animation:
		return TEXT("Animation");
	default:
		return TEXT("Unknown");
	}
}


USTRUCT(BlueprintType)
struct FBridgeAssets
{
	GENERATED_BODY()

	/** The type of asset that is contained */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Data")
	EBridgeType AssetType = EBridgeType::Unknown;

	/** If the item is a static mesh the pointer for it will be set here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Data")
	class UStaticMesh* StaticMesh = nullptr;

	/** If the item is a skeletal mesh the pointer for it will be set here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Data")
	class USkeletalMesh* SkeletalMesh = nullptr;

	/** Where to find it in the content library. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Object Data")
	FString InternalPath = "";
};

USTRUCT(BlueprintType)
struct FBridgeSelection
{
	GENERATED_BODY()

	/** The name of the selected asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Selection Data")
	FString ObjectNameInLevel = "Unknown";

	/** The name of the selected asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Selection Data")
	FTransform ObjectPositionInLevel = FTransform();

	/** The type of asset that is contained */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Selection Data")
	EBridgeType AssetType = EBridgeType::Unknown;

	/** If the item is a static mesh the pointer for it will be set here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Selection Data")
	class UStaticMesh* StaticMesh = nullptr;

	/** If the item is a skeletal mesh the pointer for it will be set here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Selection Data")
	class USkeletalMesh* SkeletalMesh = nullptr;

	/** Where to find it in the content library. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets Bridge|Selection Data")
	FTransform ItemLocation = FTransform();
};

class FAssetsBridgeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command it will initiate the Swap Operation */
	void SwapButtonClicked();
	/** This function will be bound to Command it will initiate the Make Asset Operation */
	void MakeAssetButtonClicked();
	/** This function will be bound to Command it will initiate the Export Operation */
	void ExportButtonClicked();
	/** This function will be bound to Command it will initiate the Import Operation */
	void ImportButtonClicked();
	/** This function will be bound to Command it will initialize the settings menu */
	void OpenSettingsMenu();

	/**
	 * @brief Provides a means to retrieve the current selection that the user has made.
	 */
	UPROPERTY(BlueprintReadWrite, Category="AssetsBridge Data")
	TArray<AActor*> CurrentSelection;

	UFUNCTION(BlueprintCallable, Category="AssetsBridge Data")
	FORCEINLINE TArray<AActor*> GetCurrentSelection() const { return CurrentSelection; }


	/** This function finds all iems that the user currently has selected in content browser or in level editor */
	TArray<AActor*> GetSelectedUserContext();

private:
	/** This function is needed to register menus */
	void RegisterMenus();


	/** Starts the settings plugins tab for the user to interact with settings. */
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Setup the GUI tabs required for the user to interact with Assets Bridge */
	FString AssetsBridgeContentTab = TEXT("/AssetsBridge/BPW_Settings.BPW_Settings_C");

	/** The list of commands provided by this Plugin. */
	TSharedPtr<class FUICommandList> PluginCommands;
};

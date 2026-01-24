// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetsBridge.h"

#include "ABSettings.h"
#include "AssetsBridgeStyle.h"
#include "AssetsBridgeCommands.h"
#include "AssetsBridgeTools.h"
#include "BridgeManager.h"
#include "EditorAssetLibrary.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "ISettingsModule.h"
#include "Selection.h"
#include "Blueprint/UserWidget.h"
#include "Logging/LogMacros.h"


static const FName AssetsBridgeTabName("Assets Bridge Configuration");

#define LOCTEXT_NAMESPACE "FAssetsBridgeModule"

void FAssetsBridgeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FAssetsBridgeStyle::Initialize();
	FAssetsBridgeStyle::ReloadTextures();

	FAssetsBridgeCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FAssetsBridgeCommands::Get().OpenSettingsWindow,
		FExecuteAction::CreateRaw(this, &FAssetsBridgeModule::OpenSettingsMenu),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FAssetsBridgeCommands::Get().ContentSwapAction,
		FExecuteAction::CreateRaw(this, &FAssetsBridgeModule::SwapButtonClicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FAssetsBridgeCommands::Get().MakeAssetAction,
		FExecuteAction::CreateRaw(this, &FAssetsBridgeModule::MakeAssetButtonClicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FAssetsBridgeCommands::Get().ContentExportAction,
		FExecuteAction::CreateRaw(this, &FAssetsBridgeModule::ExportButtonClicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FAssetsBridgeCommands::Get().ContentImportAction,
		FExecuteAction::CreateRaw(this, &FAssetsBridgeModule::ImportButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAssetsBridgeModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AssetsBridgeTabName,
	                                                  FOnSpawnTab::CreateRaw(
		                                                  this, &FAssetsBridgeModule::OnSpawnPluginTab))
	                        .SetDisplayName(LOCTEXT("FAssetsBridgeTabTitle", "AssetsBridge"))
	                        .SetMenuType(ETabSpawnerMenuType::Hidden);


	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "AssetsBridge",
		                                 LOCTEXT("RuntimeSettingsName", "Assets Bridge Settings"),
		                                 LOCTEXT("RuntimeSettingsDescription",
		                                         "Setup path locations for assets bridge"),
		                                 GetMutableDefault<UABSettings>());
	}
}

void FAssetsBridgeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "AssetsBridge");
	}

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FAssetsBridgeStyle::Shutdown();

	FAssetsBridgeCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AssetsBridgeTabName);
}

TSharedRef<SDockTab> FAssetsBridgeModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// Set up some failure text first in case our widget doesn't load properly...
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Failed to load content widget, fix this under {0} in {1}"),
		FText::FromString(TEXT("FWrapperHelperTempateModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("WrapperHelperTempate.cpp"))
	);

	//Declare our DockTab and assign it to the template
	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(WidgetText)
			]
		];
	//This path is contained within the WrapperHelperTemplate Content folder
	UClass* LoadedWidget = LoadObject<UClass>(NULL, *AssetsBridgeContentTab, NULL, LOAD_None, NULL);
	if (LoadedWidget)
	{
		//If successfully loaded, store it for later use.
		UUserWidget* CreatedWidget = CreateWidget(GEditor->GetEditorWorldContext().World(), LoadedWidget);
		if (CreatedWidget)
		{
			//Store our widget
			TSharedRef<SWidget> UserSlateWidget = CreatedWidget->TakeWidget();
			//Assign our widget to our tab's content
			NewDockTab->SetContent(UserSlateWidget);
		}
	}
	//Return our tab
	return NewDockTab;
}

TArray<AActor*> FAssetsBridgeModule::GetSelectedUserContext()
{
	TArray<AActor*> Actors;
	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		TArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		if (Components.Num() > 0)
		{
			Actors.Append(&Actor, 1);
		}
	}
	CurrentSelection = Actors;
	return Actors;
}

void FAssetsBridgeModule::MakeAssetButtonClicked()
{
	// Start duplication and swap, then add to array.
	bool bIsSuccessful = true;
	FString OutMessage = "Asset snatched successfully";
	TArray<FAssetData> Assets;
	UAssetsBridgeTools::GetSelectedContentBrowserItems(Assets);
	if (Assets.Num() < 1)
	{
		UAssetsBridgeTools::ShowInfoDialog(
			"Nothing selected in content browser: use Ctrl + B(Browse to asset) to select current item in the world.");
		return;
	}
	for (FAssetData Asset : Assets)
	{
		if (Asset != nullptr)
		{
			FString SourcePackagePath = UAssetsBridgeTools::GetPathWithoutExt(Asset.GetSoftObjectPath().ToString());
			FString TargetPath = UAssetsBridgeTools::GetSystemPathAsAssetPath(SourcePackagePath);
			UObject* DuplicateObject = UEditorAssetLibrary::DuplicateAsset(SourcePackagePath, TargetPath);
			if (DuplicateObject == nullptr)
			{
				bIsSuccessful = false;
				OutMessage = FString::Printf(
					TEXT("Cannot duplicate: %s to %s, does it already exist?"), *SourcePackagePath, *TargetPath);
			}
			else
			{
				TArray<UObject*> SyncObjects;
				SyncObjects.Add(DuplicateObject);
				GEditor->SyncBrowserToObjects(SyncObjects);
				UAssetsBridgeTools::ShowNotification("Asset snatched...");
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid asset detected"))
		}
	}
	if (!bIsSuccessful)
	{
		UAssetsBridgeTools::ShowNotification(OutMessage);
	}
}

void FAssetsBridgeModule::SwapButtonClicked()
{
	TArray<FAssetData> CurSelection;
	auto SelectedAssets = GetSelectedUserContext();
	UAssetsBridgeTools::GetSelectedContentBrowserItems(CurSelection);
	bool Success = false;
	FString OutMessage;
	UBridgeManager::ExecuteSwap(SelectedAssets, CurSelection, Success, OutMessage);
	if (!Success)
	{
		FText DialogText = FText::FromString(OutMessage);
		FMessageDialog::Open(EAppMsgType::Ok, DialogText);
	}
	else
	{
		UAssetsBridgeTools::ShowNotification("Object swapped...");
	}
}

void FAssetsBridgeModule::ExportButtonClicked()
{
	FString ContentLoc;
	UAssetsBridgeTools::GetExportRoot(ContentLoc);
	if (ContentLoc.Equals(""))
	{
		OpenSettingsMenu();
		return;
	}
	bool bIsSuccessful = false;
	FString OutMessage;
	UBridgeManager::StartExport(bIsSuccessful, OutMessage);
	if (!bIsSuccessful)
	{
		UAssetsBridgeTools::ShowInfoDialog(OutMessage);
	}
	else
	{
		UAssetsBridgeTools::ShowNotification(OutMessage);
	}
}

void FAssetsBridgeModule::ImportButtonClicked()
{
	FString ContentLoc;
	UAssetsBridgeTools::GetExportRoot(ContentLoc);
	if (ContentLoc.Equals(""))
	{
		OpenSettingsMenu();
		return;
	}
	bool Success = false;
	FString OutMessage;
	UBridgeManager::GenerateImport(Success, OutMessage);
	if (!Success)
	{
		FText DialogText = FText::FromString(OutMessage);
		FMessageDialog::Open(EAppMsgType::Ok, DialogText);
	}
}


void FAssetsBridgeModule::OpenSettingsMenu()
{
	// If content settings are not set show the tab else run the command
	FGlobalTabmanager::Get()->TryInvokeTab(AssetsBridgeTabName);
}

void FAssetsBridgeModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FAssetsBridgeCommands::Get().OpenSettingsWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FAssetsBridgeCommands::Get().OpenSettingsWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& ImportEntry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FAssetsBridgeCommands::Get().ContentImportAction));
				ImportEntry.SetCommandList(PluginCommands);
				FToolMenuEntry& MakeEntry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FAssetsBridgeCommands::Get().MakeAssetAction));
				MakeEntry.SetCommandList(PluginCommands);
				FToolMenuEntry& SettingsEntry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FAssetsBridgeCommands::Get().OpenSettingsWindow));
				SettingsEntry.SetCommandList(PluginCommands);
				FToolMenuEntry& SwapEntry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FAssetsBridgeCommands::Get().ContentSwapAction));
				SwapEntry.SetCommandList(PluginCommands);
				FToolMenuEntry& ExportEntry = Section.AddEntry(
					FToolMenuEntry::InitToolBarButton(FAssetsBridgeCommands::Get().ContentExportAction));
				ExportEntry.SetCommandList(PluginCommands);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAssetsBridgeModule, AssetsBridge)

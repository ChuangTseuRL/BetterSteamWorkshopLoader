#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class BetterSteamWorkshopLoaderPlugin : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow, public BakkesMod::Plugin::PluginWindow
{
	//Boilerplate
	virtual void onLoad();
	virtual void onUnload();

	// Inherited via PluginSettingsWindow
	void RenderSettings() override;
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;

	// Inherited via PluginWindow
	virtual void Render() override;
	virtual std::string GetMenuName() override;
	virtual std::string GetMenuTitle() override;
	virtual bool ShouldBlockInput() override;
	virtual bool IsActiveOverlay() override;
	virtual void OnOpen() override;
	virtual void OnClose() override;

	// BetterSteamWorkshopLoaderPlugin
	void SaveData();

	std::string ProcessWorkshopFolder(std::filesystem::path workshopFolder);

	bool isWindowOpen_ = false;
	std::string menuTitle_ = "BetterSteamWorkshopLoader";
	std::string pluginNiceName_ = "Better Steam Workshop Loader";

	bool isSteam_ = false;
	bool isFullyLoaded_ = false;
	std::string loadErrorMessage_;

	std::unique_ptr<CVarWrapper> columnsCountCVar;
	std::shared_ptr<int> columnsCount_;
	std::unique_ptr<CVarWrapper> previewSizeCVar;
	std::shared_ptr<int> previewSize_;

	std::filesystem::path steamRlWorkshopFolder_;
	std::filesystem::path pluginDataPath_;

	std::vector<std::string> sortedWorkshopIds_;

	std::map<std::string, std::shared_ptr<ImageWrapper>> workshopImages_;
	std::map<std::string, std::string> workshopTitles_;
	std::map<std::string, std::filesystem::path> workshopUdkFiles_;
	std::map<std::string, bool> workshopIsNewThisSession_;

	std::shared_ptr<ImageWrapper> noPreviewImage_;
};
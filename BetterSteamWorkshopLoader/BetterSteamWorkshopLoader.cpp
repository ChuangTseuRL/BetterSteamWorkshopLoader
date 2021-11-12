#include "pch.h"
#include "BetterSteamWorkshopLoader.h"

#include <fstream>

BAKKESMOD_PLUGIN(BetterSteamWorkshopLoaderPlugin, "Better Steam Workshop Loader", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

std::string ReadFileFirstLine(std::filesystem::path filepath) {
	std::string firstLine;
	std::ifstream file(filepath);
	std::getline(file, firstLine);
	return firstLine;
}

std::vector<std::string> SplitStringComma(std::string str) {
	std::vector<std::string> split;
	if (!str.empty()) {
		size_t prevFirstChar = 0;
		for (size_t i = 0; i < str.size(); ++i) {
			if (str[i] == ',') {
				split.push_back(str.substr(prevFirstChar, i - prevFirstChar));
				prevFirstChar = i + 1;
			}
		}
		split.push_back(str.substr(prevFirstChar, str.size() - prevFirstChar));
	}
	return split;
}

std::vector<std::filesystem::path> SplitPath(std::filesystem::path path) {
	std::vector<std::filesystem::path> split;
	for (const auto& sub : path) {
		split.push_back(sub.string());
	}
	return split;
}

std::filesystem::path NthParentPath(std::filesystem::path path, int nth) {
	for (int i = 0; i < nth; ++i) {
		path = path.parent_path();
	}
	return path;
}

std::filesystem::path GetSteamappsPathFromRLExecutable(std::filesystem::path rlExe) {
	// The exe path should look like this "[...]/steamapps/common/rocketleague/Binaries/Win64/RocketLeague.exe"
	// Future changes to the RL exe path location shouldn't matter as long as we can find the "/steamapps/common/" subfolders.
	std::vector<std::filesystem::path> splitPath = SplitPath(rlExe);
	const int splitPathCount = (int)splitPath.size();
	for (int i = splitPathCount - 2; i >= 0; --i) {
		if (splitPath[i] == "steamapps" && splitPath[static_cast<std::size_t>(i) + 1] == "common") {
			return NthParentPath(rlExe, splitPathCount - i - 1);
		}
	}
	return {};
}

void BetterSteamWorkshopLoaderPlugin::onLoad()
{
	_globalCvarManager = cvarManager;

	isSteam_ = gameWrapper->IsUsingSteamVersion();
#if !IS_DEV_ON_EPIC
	if (!isSteam_) {
		cvarManager->log("This plugin only works with the steam version.");
		return;
	}
#endif

	columnsCount_ = std::make_shared<int>(5);
	columnsCountCVar = std::make_unique<CVarWrapper>(cvarManager->registerCvar("bswl_columns_count", "5", "", false, true, 0, true, 10, true));
	columnsCountCVar->bindTo(columnsCount_);
	previewSize_ = std::make_shared<int>(0);
	previewSizeCVar = std::make_unique<CVarWrapper>(cvarManager->registerCvar("bswl_preview_size", "0", "", false, true, 0, true, 2, true));
	previewSizeCVar->bindTo(previewSize_);

	pluginDataPath_ = gameWrapper->GetDataFolder() / "BetterSteamWorkshopLoader";

	std::filesystem::path idsSaveFilePath = pluginDataPath_ / "ids.sav";

	std::string savedIdsStr = ReadFileFirstLine(idsSaveFilePath);
	cvarManager->log(fmt::format("savedIdsStr: {}", savedIdsStr));

	std::vector<std::string> savedIds = SplitStringComma(savedIdsStr);

	// Will always be true in retail version at this point
	if (isSteam_) {
		// Get (Steam) exe path. Works :)
		WCHAR modulePathW[MAX_PATH];
		GetModuleFileNameW(NULL, modulePathW, MAX_PATH);
		cvarManager->log(fmt::format(L"modulePath: {}", modulePathW));

		std::filesystem::path steamappsPath = GetSteamappsPathFromRLExecutable(modulePathW);
		if (steamappsPath.empty()) {
			cvarManager->log("Error! Could not find the steamapps folder from the modulePath.");
			loadErrorMessage_ = "ERROR! Could not find the steamapps folder from the running RocketLeague.exe path.";
			return;
		}
		else {
			cvarManager->log(fmt::format("Found steamapps folder at path: {}", steamappsPath.string()));
		}
		steamRlWorkshopFolder_ = steamappsPath / "workshop" / "content" / "252950";
	}
#if IS_DEV_ON_EPIC
	if (!isSteam_) {
		// If you enable IS_DEV_ON_EPIC, then set your actual Steam RL workshop folder path here.
		const char* mySteamWorkshopPathStr = "C:\\Program Files (x86)\\Steam\\steamapps\\workshop\\content\\252950";
		steamRlWorkshopFolder_ = mySteamWorkshopPathStr;
	}
#endif
	cvarManager->log(fmt::format("Using Steam RL workshop folder path: {}", steamRlWorkshopFolder_.string()));

	noPreviewImage_ = std::make_shared<ImageWrapper>(pluginDataPath_ / "no_preview.jpg", false, true);

	// Process all the found workshop folder: will validate the folder, then load all the required resources for each workshop item
	std::vector<std::string> foundIds;
	for (auto const& dirEntry : std::filesystem::directory_iterator(steamRlWorkshopFolder_)) {
		if (dirEntry.is_directory()) {
			std::string workshopId = ProcessWorkshopFolder(dirEntry.path());
			if (!workshopId.empty()) {
				foundIds.push_back(workshopId);
			}
		}
	}

	// Respect saved ordering while adding newly discovered workshops
	// Add newly discovered at top
	for (const std::string& foundId : foundIds) {
		if (std::find(savedIds.cbegin(), savedIds.cend(), foundId) == savedIds.cend()) {
			sortedWorkshopIds_.push_back(foundId);
			workshopIsNewThisSession_[foundId] = true;
		}
	}
	// Add saved just after, in saved order, only if it still exists locally
	for (const std::string& savedId : savedIds) {
		if (std::find(foundIds.cbegin(), foundIds.cend(), savedId) != foundIds.cend()) {
			sortedWorkshopIds_.push_back(savedId);
			workshopIsNewThisSession_[savedId] = false;
		}
	}

	isFullyLoaded_ = true;
}

void BetterSteamWorkshopLoaderPlugin::onUnload()
{
	// Let's not override any saved data when the pluggin was not actually loaded.
	if (!isFullyLoaded_) {
		return;
	}

	SaveData();

	isFullyLoaded_ = false;
}

void BetterSteamWorkshopLoaderPlugin::SaveData()
{
	std::filesystem::path idsSaveFilePath = pluginDataPath_ / "ids.sav";

	std::ofstream idsSaveFile(idsSaveFilePath);
	if (!idsSaveFile.is_open()) {
		cvarManager->log("Error opening the saved Ids file for saving. Won't save.");
		return;
	}

	if (sortedWorkshopIds_.size() == 0) {
		return;
	}

	std::string savedIdsStr = sortedWorkshopIds_[0];
	for (int i = 1; i < sortedWorkshopIds_.size(); ++i) {
		savedIdsStr += "," + sortedWorkshopIds_[i];
	}

	cvarManager->log(fmt::format("Saving savedIdsStr: {}", savedIdsStr));
	idsSaveFile.write(savedIdsStr.c_str(), savedIdsStr.size());
}

bool IsValidWorkshopFolderName(std::filesystem::path workshopFolder) {
	// Basic verification, folder name needs to be numbers only
	std::string folderName = workshopFolder.filename().string();
	return std::all_of(folderName.cbegin(), folderName.cend(), [](char c) { return std::isdigit(c); });
}

std::filesystem::path GetLocalWorkshopUdkFile(std::filesystem::path workshopFolder) {
	// Returns the first udk found. Empty path if none is present.
	for (auto const& wsDirEntry : std::filesystem::directory_iterator(workshopFolder)) {
		if (wsDirEntry.path().extension() == ".udk") {
			return wsDirEntry.path();
		}
	}
	return {};
}

std::filesystem::path GetLocalWorkshopPreviewFile(std::filesystem::path workshopFolder) {
	// Returns the first preview image found (png and jpg only). Empty path if none is present.
	for (auto const& wsDirEntry : std::filesystem::directory_iterator(workshopFolder)) {
		const std::filesystem::path& ext = wsDirEntry.path().extension();
		if (ext == ".jpg" || ext == ".png") {
			return wsDirEntry.path();
		}
	}
	return {};
}

// Returns the workshop id. Empty string if workshop folder is invalid in any way.
std::string BetterSteamWorkshopLoaderPlugin::ProcessWorkshopFolder(std::filesystem::path workshopFolder)
{
	if (!IsValidWorkshopFolderName(workshopFolder)) {
		return {};
	}

	std::filesystem::path udkFile = GetLocalWorkshopUdkFile(workshopFolder);
	if (udkFile.empty()) {
		cvarManager->log(fmt::format("Workshop folder at {} does not contain any udk map file. Skip.", workshopFolder.string()));
		return {};
	}

	std::string folderName = workshopFolder.filename().string();
	std::string workshopId = folderName;

	cvarManager->log(fmt::format("Found valid workshop folder at {} with udk map file: {}", workshopFolder.string(), udkFile.filename().string()));
	workshopUdkFiles_[workshopId] = udkFile;

	std::filesystem::path workshopDatabasePath = pluginDataPath_ / "database";
	std::filesystem::path workshopDatabasePreviewPath = workshopDatabasePath / (workshopId + ".jpg");
	std::filesystem::path workshopDatabaseTitlePath = workshopDatabasePath / (workshopId + ".txt");

	// Technically both always exist at the same time if I don't mess things up when creating the database.
	bool hasDatabaseEntry = std::filesystem::exists(workshopDatabasePreviewPath) && std::filesystem::exists(workshopDatabaseTitlePath);
	if (hasDatabaseEntry) {
		workshopImages_[workshopId] = std::make_shared<ImageWrapper>(workshopDatabasePreviewPath, false, true);
		workshopTitles_[workshopId] = ReadFileFirstLine(workshopDatabaseTitlePath);
	}
	else {
		std::filesystem::path localWorkshopPreviewFile = GetLocalWorkshopPreviewFile(workshopFolder);
		if (!localWorkshopPreviewFile.empty()) {
			workshopImages_[workshopId] = std::make_shared<ImageWrapper>(localWorkshopPreviewFile, false, true);
		}
		else {
			workshopImages_[workshopId] = noPreviewImage_;
		}

		workshopTitles_[workshopId] = fmt::format("[{}] #{}", udkFile.filename().string(), workshopId);
	}

	return workshopId;
}


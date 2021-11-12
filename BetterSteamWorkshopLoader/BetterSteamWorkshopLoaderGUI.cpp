#include "pch.h"
#include "BetterSteamWorkshopLoader.h"

#include "imgui/imgui_internal.h"

std::string BetterSteamWorkshopLoaderPlugin::GetPluginName() {
	return pluginNiceName_;
}

void BetterSteamWorkshopLoaderPlugin::SetImGuiContext(uintptr_t ctx) {
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void BetterSteamWorkshopLoaderPlugin::RenderSettings() {
#if !IS_DEV_ON_EPIC
	if (!isSteam_) {
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "PLUGIN DISABLED. ONLY WORKS WITH STEAM VERSION.");
		return;
	}
#endif
	if (!isFullyLoaded_) {
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), loadErrorMessage_.c_str());
		return;
	}

	if (ImGui::Button("Open Workshop Loader")) {
		gameWrapper->Execute([this](GameWrapper* gw) {
			cvarManager->executeCommand("togglemenu " + menuTitle_);
			});
	}

	ImGui::Separator();
	ImGui::Separator();

	// Very simple "set keybind" implementation. Basically a simple shortcut to avoid typing `bind F3 "togglemenu BetterSteamWorkshopLoader"`.
	static char keybindBuf[64] = "F3";
	if (ImGui::Button("Set Keybind")) {
		cvarManager->setBind(keybindBuf, "togglemenu " + menuTitle_);
	}
	ImGui::SameLine();
	ImGui::PushItemWidth(60);
	ImGui::InputText("##keybind", keybindBuf, IM_ARRAYSIZE(keybindBuf));
	ImGui::PopItemWidth();

	ImGui::Separator();

	// Columns count selection
	int newColumnsCount = *columnsCount_;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Columns count: ");
	ImGui::SameLine();
	ImGui::PushItemWidth(80);
	ImGui::InputInt("##ColumnsCountInput", &newColumnsCount);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("(max 10)");
	newColumnsCount = std::clamp(newColumnsCount, 1, 10);
	if (newColumnsCount != *columnsCount_) {
		*columnsCount_ = newColumnsCount;
		columnsCountCVar->setValue(newColumnsCount);
	}

	// Workshop preview image size selection
	int newPreviewSize = *previewSize_;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Preview size: "); ImGui::SameLine();
	ImGui::RadioButton("small", &newPreviewSize, 0); ; ImGui::SameLine();
	ImGui::RadioButton("medium", &newPreviewSize, 1); ; ImGui::SameLine();
	ImGui::RadioButton("large", &newPreviewSize, 2);
	if (newPreviewSize != *previewSize_) {
		*previewSize_ = newPreviewSize;
		previewSizeCVar->setValue(newPreviewSize);
	}
}

void BetterSteamWorkshopLoaderPlugin::Render()
{
#if !IS_DEV_ON_EPIC
	if (!isSteam_) {
		return;
	}
#endif
	if (!isFullyLoaded_) {
		return;
	}

	const int windowWidth = 1120;
	const int windowHeight = 700;
	ImGui::SetNextWindowSize({ (float)windowWidth, (float)windowHeight }, ImGuiCond_FirstUseEver);

	if (!ImGui::Begin(pluginNiceName_.c_str(), &isWindowOpen_, ImGuiWindowFlags_None))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	int imgWidth;
	int imgHeight;
	if (*previewSize_ == 0) {
		imgWidth = 192;
		imgHeight = 108;
	}
	else if (*previewSize_ == 1) {
		imgWidth = 240;
		imgHeight = 135;
	}
	else {
		imgWidth = 288;
		imgHeight = 162;
	}
	const int gridChildHeight = imgHeight + 56;

	const int moveButtonPadding = 8;

	if (ImGui::Button("Exit to Main Menu (freeplay/workshop only)")) {
		gameWrapper->Execute([&](GameWrapper* gw) {
			if (gw->IsInFreeplay()) {
				gw->ExecuteUnrealCommand("start MENU_Main_p?close");
				cvarManager->executeCommand("closemenu " + menuTitle_);
			}
			});
	}

	ImGui::BeginChild("WorkshopList");

	std::vector<std::string> updatedSortedIds;

	ImGui::Columns(*columnsCount_, NULL, false);
	for (int i = 0; i < sortedWorkshopIds_.size(); ++i) {
		const std::string& workshopId = sortedWorkshopIds_[i];

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
		ImGui::BeginChild(workshopId.c_str(), ImVec2(0, (float)gridChildHeight), true);

		const auto& workshopImage = workshopImages_[workshopId];
		if (auto pTex = workshopImage->GetImGuiTex()) {
			auto rect = workshopImage->GetSizeF();
			ImGui::Image(pTex, { (float)imgWidth, (float)imgHeight });
		}

		ImGui::SameLine((float)moveButtonPadding);

		int moveDir = 0;
		const float hueFactor = 3.0f / 7.0f;
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(hueFactor, 0.6f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(hueFactor, 0.7f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(hueFactor, 0.8f, 0.8f));
		if (ImGui::Button(" < ")) {
			moveDir = -1;
		}
		ImGui::SameLine(0, 4);
		if (ImGui::Button(" > ")) {
			moveDir = 1;
		}
		ImGui::PopStyleColor(3);

		if (workshopIsNewThisSession_[workshopId]) {
			const float newHueFactor = 6.0f / 7.0f;
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(newHueFactor, 0.6f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(newHueFactor, 0.7f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(newHueFactor, 0.8f, 0.8f));

			ImGui::SameLine();
			ImGui::Button("NEW");
			ImGui::PopStyleColor(3);
		}

		if (moveDir != 0) {
			updatedSortedIds = sortedWorkshopIds_;

			int newIndex = i + moveDir;

			// Shift everything left, first becomes last
			if (newIndex < 0) {
				std::rotate(updatedSortedIds.begin(), updatedSortedIds.begin() + 1, updatedSortedIds.end());
			}
			// Shift everything right, last becomes first
			else if (newIndex == sortedWorkshopIds_.size()) {
				std::rotate(updatedSortedIds.rbegin(), updatedSortedIds.rbegin() + 1, updatedSortedIds.rend());
			}
			else {
				std::swap(updatedSortedIds[newIndex], updatedSortedIds[i]);
			}
		}

		ImGui::BeginGroup();
		ImGui::Text(workshopTitles_[workshopId].c_str());
		if (ImGui::Button("Play", ImVec2(-FLT_MIN, 0.0f))) {
			gameWrapper->Execute([&](GameWrapper* gw) {
				// Use full path instead of Steam format for load_workshop. Should work 100% of the time this way.
				std::string loadWorkshopCommand = fmt::format(
					"load_workshop \"{}\"", workshopUdkFiles_[workshopId].string());
				cvarManager->log(loadWorkshopCommand);

				cvarManager->executeCommand(loadWorkshopCommand);
				cvarManager->executeCommand("closemenu " + menuTitle_);
				});
		}
		ImGui::EndGroup();

		ImGui::EndChild();
		ImGui::PopStyleVar();

		ImGui::NextColumn();
	}
	ImGui::Columns(1);

	if (!updatedSortedIds.empty()) {
		sortedWorkshopIds_ = updatedSortedIds;

		SaveData();
	}

	ImGui::EndChild();

	ImGui::End();

	if (!isWindowOpen_)
	{
		cvarManager->executeCommand("togglemenu " + menuTitle_);
	}
}

// Name of the menu that is used to toggle the window.
std::string BetterSteamWorkshopLoaderPlugin::GetMenuName()
{
	return menuTitle_;
}

// Title to give the menu
std::string BetterSteamWorkshopLoaderPlugin::GetMenuTitle()
{
	return pluginNiceName_;
}

// Should events such as mouse clicks/key inputs be blocked so they won't reach the game
bool BetterSteamWorkshopLoaderPlugin::ShouldBlockInput()
{
	return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

// Return true if window should be interactive
bool BetterSteamWorkshopLoaderPlugin::IsActiveOverlay()
{
	return true;
}

// Called when window is opened
void BetterSteamWorkshopLoaderPlugin::OnOpen()
{
	isWindowOpen_ = true;
}

// Called when window is closed
void BetterSteamWorkshopLoaderPlugin::OnClose()
{
	// We don't need to display the NEW indicator anymore once the window has been closed once
	for (auto& [key, value] : workshopIsNewThisSession_) {
		value = false;
	}

	isWindowOpen_ = false;
}

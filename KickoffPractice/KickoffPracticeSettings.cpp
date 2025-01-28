#include "pch.h"
#include "KickoffPractice.h"

void KickoffPractice::RenderSettings()
{
	ImGui::Checkbox("Enable plugin", &pluginEnabled);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Enable or disable the plugin");
	}
	ImGui::NewLine();
	ImGui::SliderFloat("Time before back to normal", &this->timeAfterBackToNormal, 0.0f, 3.0f, "%.3f seconds");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How long you stay in \"kickoff mode\" after someone hit the ball. This also affects how long the recording lasts after hitting the ball.");
	}
	ImGui::NewLine();
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Should a bot be spawned when recording inputs");
	}
	ImGui::Separator();

#pragma region browseRecord

	ImGui::TextUnformatted("Select a folder to record kickoffs in");
	ImGui::SameLine();
	if (ImGui::Button(".."))
	{
		ImGui::OpenPopup("browseRecord");
	}
	ImGui::SetNextWindowSize(ImVec2(500, 500));

	if (ImGui::BeginPopup("browseRecord", ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar))
	{
		std::string inputPath = botMenu.main();
		if (inputPath != "")
		{
			recordedKickoffFolder = inputPath;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	std::string recordedKickoffFolder_str = recordedKickoffFolder.string();
	if (ImGui::InputText("##recordedKickoffFolder", &recordedKickoffFolder_str))
		recordedKickoffFolder = recordedKickoffFolder_str;

#pragma endregion
	for (int i = 0; i < 5; i++)
	{
		if (ImGui::Button(("Record " + getKickoffName(i)).c_str()))
		{
			gameWrapper->Execute([this, i](GameWrapper* gw)
				{
					cvarManager->executeCommand("kickoff_train " + std::to_string(i + 1) + " true;closemenu settings");
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(("Record input for the " + getKickoffName(i) + " kickoff").c_str());
		}
		if (i != 4)ImGui::SameLine();
	}

	ImGui::Separator();

#pragma region browseKickoff

	ImGui::TextUnformatted("Select a folder to browse for kickoffs");
	ImGui::SameLine();
	if (ImGui::Button("..."))
	{
		ImGui::OpenPopup("browseKickoff");
	}

	ImGui::SetNextWindowSize(ImVec2(500, 500));


	if (ImGui::BeginPopup("browseKickoff", ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar))
	{
		std::string inputPath = recordMenu.main();
		if (inputPath != "")
		{
			botKickoffFolder = inputPath;

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	std::string botKickoffFolder_str = botKickoffFolder.string();
	if (ImGui::InputText("##botKickoffFolder", &botKickoffFolder_str))
		botKickoffFolder = botKickoffFolder_str;


	if (ImGui::Button("Validate"))
	{
		readKickoffFiles();
		LOG("{}", loadedInputs.size());
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Validate changes");
	}

	if (loadedKickoffIndices.size() == 0)
	{
		ImGui::TextColored(ImVec4(255, 0, 0, 255), "No kickoff selected !");
	}

#pragma endregion

	ImGui::BeginGroup();
	ImGui::TextUnformatted("");

	const ImGuiWindowFlags child_flags = 0;
	const ImGuiID child_id = ImGui::GetID((void*)(intptr_t)0);
	const bool child_is_visible = ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true, child_flags);

	size_t count = loadedInputs.size();
	// TODO: Duplication
	const char* items[] = { "Unused", "Right Corner", "Left Corner", "Back Right", "Back Left", "Back Center" };
	bool isChanged = false;
	if (child_is_visible)
	{
		ImGui::Indent(5);
		for (int i = 0; i < count; i++)
		{
			ImGui::PushID(i);
			isChanged = isChanged || ImGui::Combo(loadedInputs[i].name.c_str(), &states[i], items, IM_ARRAYSIZE(items));
			ImGui::PopID();
		}
	}

	float scroll_y = ImGui::GetScrollY();
	float scroll_max_y = ImGui::GetScrollMaxY();
	ImGui::EndChild();
	ImGui::EndGroup();

	if (isChanged)
	{
		updateLoadedKickoffIndices();
		writeConfigFile(configPath / "config.cfg");
	}

	if (nbCarBody == -1)
		return;
	if (ImGui::Combo("Bot car body", &selectedCarUI, carNames, nbCarBody))
	{
		this->botCarID = carBodyIDs[selectedCarUI];
	}
}

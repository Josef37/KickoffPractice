#include "pch.h"
#include "KickoffPractice.h"

static void SpacedSeparator()
{
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}

void KickoffPractice::RenderSettings()
{
	ImGui::Checkbox("Enable plugin", &pluginEnabled);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Enable or disable the plugin");

	ImGui::NewLine();
	ImGui::SliderFloat("Time before back to normal", &this->timeAfterBackToNormal, 0.0f, 3.0f, "%.3f seconds");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How long you stay in \"kickoff mode\" after someone hit the ball. This also affects how long the recording lasts after hitting the ball.");

	SpacedSeparator();

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
			ImGui::SetTooltip(("Record input for the " + getKickoffName(i) + " kickoff").c_str());

		if (i != 4)ImGui::SameLine();
	}

	SpacedSeparator();

	if (ImGui::Button("Reload files"))
		readKickoffFiles();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Reload recorded kickoffs");

	if (loadedKickoffPositions.size() == 0)
	{
		ImGui::TextColored(ImVec4(255, 0, 0, 255), "No kickoff selected!");
	}

	ImGui::Spacing();
	// TODO: Duplication
	const char* items[] = { "Unused", "Right Corner", "Left Corner", "Back Right", "Back Left", "Back Center" };
	bool isChanged = false;
	if (ImGui::BeginChild("LoadedFiles", ImVec2(0, 0), true))
	{
		ImGui::Indent(5);
		ImGui::PushItemWidth(ImGui::GetFontSize() * 15.f);

		for (int i = 0; i < loadedKickoffs.size(); i++)
		{
			isChanged = isChanged || ImGui::Combo(loadedKickoffs[i].name.c_str(), &states[i], items, IM_ARRAYSIZE(items));
		}
	}
	ImGui::EndChild();

	if (isChanged)
	{
		updateLoadedKickoffPositions();
		writeConfigFile();
	}
}

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
	ImGui::Spacing();

	ImGui::Checkbox("Enable plugin", &pluginEnabled);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Enable or disable the plugin");

	ImGui::Spacing();

	ImGui::SliderFloat("Time before back to normal", &this->timeAfterBackToNormal, 0.0f, 3.0f, "%.3f seconds");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How long you stay in \"kickoff mode\" after someone hit the ball. This also affects how long the recording lasts after hitting the ball.");

	SpacedSeparator();

	ImGui::Text("Training");
	ImGui::Spacing();

	for (int i = -1; i < 5; i++)
	{
		auto command = TRAIN_COMMAND + " " + std::to_string(i + 1);
		auto label = "Train " + getKickoffName(i);

		if (i == -1)
		{
			command = "kickoff_train";
			label = "Train All";
		}

		if (i >= 0) ImGui::SameLine();

		if (ImGui::Button(label.c_str()))
			gameWrapper->Execute([this, command](GameWrapper* gw)
				{
					cvarManager->executeCommand(command + ";closemenu settings");
				});
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(command.c_str());
	}

	SpacedSeparator();

	ImGui::Text("Recording");
	ImGui::Spacing();

	for (int i = 0; i < 5; i++)
	{
		if (i > 0) ImGui::SameLine();

		auto command = RECORD_COMMAND + " " + std::to_string(i + 1);
		auto label = "Record " + getKickoffName(i);

		if (ImGui::Button(label.c_str()))
			gameWrapper->Execute([this, command](GameWrapper* gw)
				{
					cvarManager->executeCommand(command + ";closemenu settings");
				});
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(command.c_str());

	}
	ImGui::Spacing();
	if (ImGui::Button("Save last attempt"))
		gameWrapper->Execute([this](GameWrapper* gw)
			{
				cvarManager->executeCommand(SAVE_COMMAND);
			});
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Save the last kickoff you made. When starting a recording it is saved automatically.");

	SpacedSeparator();

	if (ImGui::Button("Reload files"))
		readKickoffFiles();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Reload recorded kickoffs");

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
		writeConfigFile();
	}
}

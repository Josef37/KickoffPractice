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

	for (int position = -1; position < 5; position++)
	{
		auto command = TRAIN_COMMAND + " " + std::to_string(position + 1);
		auto label = "Train " + getKickoffName(position);

		if (position == -1)
		{
			command = "kickoff_train";
			label = "Train All";
		}

		if (position >= 0) ImGui::SameLine();

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

	for (int position = 0; position < 5; position++)
	{
		if (position > 0) ImGui::SameLine();

		auto command = RECORD_COMMAND + " " + std::to_string(position + 1);
		auto label = "Record " + getKickoffName(position);

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

	if (ImGui::Button("Reload kickoffs"))
		readKickoffFiles();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Reload recorded kickoffs from files.");

	ImGui::Spacing();

	bool changedActiveKickoffs = false;

	// TODO: Don't compute every loop.
	std::map<int, std::vector<RecordedKickoff*>> kickoffsByPosition;
	for (auto& kickoff : loadedKickoffs)
		kickoffsByPosition[kickoff.position].push_back(&kickoff);

	for (int position = 0; position < 5; position++)
	{
		ImGui::Text(getKickoffName(position).c_str());

		if (kickoffsByPosition[position].empty())
			ImGui::Text("(no kickoffs recorded)");

		for (auto& kickoff : kickoffsByPosition[position])
		{
			if (ImGui::Checkbox(kickoff->name.c_str(), &kickoff->isActive))
				changedActiveKickoffs = true;

			ImGui::SameLine();
			if (ImGui::Button(("Replay##" + kickoff->name).c_str()))
				gameWrapper->Execute([this, &kickoff](GameWrapper* gw)
					{
						cvarManager->executeCommand(REPLAY_COMMAND + " \"" + kickoff->name + "\"");
					}
				);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Replay a kickoff. Spawns a bot that replays the same recording. Only changes the bot car body.");
		}
		ImGui::Spacing();
	}

	if (changedActiveKickoffs)
		writeConfigFile();
}

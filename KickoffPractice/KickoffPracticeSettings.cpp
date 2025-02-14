#include "pch.h"
#include "KickoffPractice.h"

static void SpacedSeparator()
{
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}

void KickoffPractice::CommandButton(const std::string& label, const std::string& command)
{
	if (ImGui::Button(label.c_str()))
		this->gameWrapper->Execute([this, command](...)
			{
				this->cvarManager->executeCommand(command + ";closemenu settings");
			});
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(command.c_str());
}

void KickoffPractice::RenderSettings()
{
	ImGui::Spacing();

	if (ImGui::Checkbox("Enable plugin", &pluginEnabled))
		cvarManager->getCvar(CVAR_ENABLED).setValue(pluginEnabled);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Enable or disable the plugin");

	ImGui::Spacing();

	ImGui::SetNextItemWidth(300);
	if (ImGui::DragFloat("Time before back to normal", &timeAfterBackToNormal, 0.002f, 0.0f, FLT_MAX, "%.1f seconds"))
		cvarManager->getCvar(CVAR_BACK_TO_NORMAL).setValue(timeAfterBackToNormal);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How long you stay in \"kickoff mode\" after someone hit the ball. This also affects how long the recording lasts after hitting the ball.");

	ImGui::Spacing();

	if (ImGui::Checkbox("Restart on Freeplay Reset", &restartOnTrainingReset))
		cvarManager->getCvar(CVAR_RESTART_ON_RESET).setValue(restartOnTrainingReset);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Repeats the last command when resetting freeplay, i.e. using the \"Rest Ball\" binding.\n"
			"When used during a kickoff just resets freeplay. To restart in this case, reset twice.");

	if (ImGui::Checkbox("Auto-Restart", &autoRestart))
		cvarManager->getCvar(CVAR_AUTO_RESTART).setValue(autoRestart);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Automatically repeats the last command. Break out of auto-restart by resetting freeplay.");

	if (ImGui::Checkbox("Show Indicator", &showIndicator))
		cvarManager->getCvar(CVAR_SHOW_INDICATOR).setValue(showIndicator);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Display a text showing what the plugin is currently doing.");

	if (ImGui::Checkbox("Show Speedflip Trainer", &showSpeedFlipTrainer))
		cvarManager->getCvar(CVAR_SPEEDFLIP_TRAINER).setValue(showSpeedFlipTrainer);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"If you want to change more display options for the Speedflip Trainer overlay, install the Speedflip Trainer plugin and reload this plugin.\n"
			"This setting here is not shared with the Speedflip Trainer plugin.\n"
			"If you enable this plugin before the Speedflip Trainer, the settings are not synchronized."
		);

	ImGui::Spacing();

	if (ImGui::Button("Reset Training/Recording"))
		gameWrapper->Execute([this](...) { this->reset(); });
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Reset back to normal freeplay.");

	SpacedSeparator();

	ImGui::Text("Training");
	ImGui::Spacing();

	for (int i = 0; i < Utils::allKickoffPositions.size(); i++)
	{
		auto position = Utils::allKickoffPositions[i];

		if (i > 0) ImGui::SameLine();

		auto label = "Train " + Utils::getKickoffPositionName(position);
		auto command = TRAIN_COMMAND + " " + getKickoffArg(position);

		CommandButton(label, command);
	}
	ImGui::Spacing();

	CommandButton("Train Selected", TRAIN_COMMAND);

	for (KickoffPosition position : Utils::allKickoffPositions)
	{
		ImGui::SameLine();

		bool active = activePositions.contains(position);
		auto positionName = Utils::getKickoffPositionName(position);
		if (ImGui::Checkbox(positionName.c_str(), &active))
		{
			if (active) activePositions.insert(position);
			else activePositions.erase(position);

			cvarManager->getCvar(CVAR_ACTIVE_POSITIONS).setValue(getActivePositionsMask());
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Select All"))
	{
		activePositions.insert({
			KickoffPosition::CornerRight,
			KickoffPosition::CornerLeft,
			KickoffPosition::BackRight,
			KickoffPosition::BackLeft,
			KickoffPosition::BackCenter });
		cvarManager->getCvar(CVAR_ACTIVE_POSITIONS).setValue(getActivePositionsMask());
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear All"))
	{
		activePositions.clear();
		cvarManager->getCvar(CVAR_ACTIVE_POSITIONS).setValue(getActivePositionsMask());
	}

	SpacedSeparator();

	ImGui::Text("Recording");
	ImGui::Spacing();

	for (int i = 0; i < Utils::allKickoffPositions.size(); i++)
	{
		auto position = Utils::allKickoffPositions[i];

		if (i > 0) ImGui::SameLine();

		auto command = RECORD_COMMAND + " " + getKickoffArg(position);
		auto label = "Record " + Utils::getKickoffPositionName(position);

		CommandButton(label, command);
	}
	ImGui::Spacing();
	if (ImGui::Button("Save Last Attempt"))
		gameWrapper->Execute([this](...)
			{
				cvarManager->executeCommand(SAVE_COMMAND);
			});
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Save the last kickoff you made. Recordings are saved automatically.");

	SpacedSeparator();

	ImGui::Text("Select Kickoffs to use for Training");
	ImGui::Spacing();

	if (ImGui::Button("Reload kickoffs"))
		readKickoffsFromFile();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Reload recorded kickoffs from files.");

	ImGui::Spacing();

	bool changedActiveKickoffs = false;

	for (KickoffPosition position : Utils::allKickoffPositions)
	{
		auto positionName = Utils::getKickoffPositionName(position);
		ImGui::Text(positionName.c_str());

		if (kickoffIndexByPosition[position].empty())
		{
			ImGui::Text("(no kickoffs recorded)");
			ImGui::Spacing();
			continue;
		}

		for (auto index : kickoffIndexByPosition[position])
		{
			auto& kickoff = loadedKickoffs[index];

			ImGui::PushID(kickoff.name.c_str());

			if (index == this->currentKickoffIndex)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));

			if (ImGui::Checkbox(kickoff.name.c_str(), &kickoff.isActive))
				changedActiveKickoffs = true;

			if (index == this->currentKickoffIndex)
				ImGui::PopStyleColor();

			auto label = "Replay";
			auto command = REPLAY_COMMAND + " \"" + kickoff.name + "\"";

			ImGui::SameLine();
			CommandButton(label, command);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Replay a kickoff. Spawns a bot that replays the same recording. Only changes the bot car body.");

			ImGui::SameLine();
			if (ImGui::Button("Rename"))
			{
				tempName = kickoff.name;
				ImGui::OpenPopup("Rename Recording");
			}

			ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

			if (ImGui::BeginPopupModal("Rename Recording", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20);
				ImGui::InputText("", &tempName);
				ImGui::Spacing();

				if (ImGui::Button("Rename", ImVec2(120, 0)))
				{
					this->renameKickoffFile(kickoff.name, tempName, [&]()
						{
							changedActiveKickoffs = true;
							ImGui::CloseCurrentPopup();
						});
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

				ImGui::EndPopup();
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
			if (ImGui::Button("Delete"))
				ImGui::OpenPopup("Delete Recording");
			ImGui::PopStyleColor();

			ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

			if (ImGui::BeginPopupModal("Delete Recording", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				auto text = std::format("Confirm you want to delete this recording:\n{}", kickoff.name);
				ImGui::Text(text.c_str());
				ImGui::Spacing();
				ImGui::Text("This operation cannot be undone!");
				ImGui::Spacing();

				if (ImGui::Button("Delete", ImVec2(120, 0)))
				{
					this->deleteKickoffFile(kickoff.name, [&]()
						{
							changedActiveKickoffs = true;
							ImGui::CloseCurrentPopup();
						});
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

				ImGui::EndPopup();
			}

			ImGui::PopID();
		}
		ImGui::Spacing();
	}

	if (changedActiveKickoffs)
		kickoffStorage->saveActiveKickoffs(loadedKickoffs);
}

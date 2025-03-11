#include "pch.h"
#include "KickoffPractice.h"

void KickoffPractice::RenderSettings()
{
	if (ImGui::BeginTabBar("KickoffPractice"))
	{
		if (ImGui::BeginTabItem("Settings"))
		{
			RenderSettingsTab(); 
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Speedflip"))
		{
			speedFlipTrainer->RenderSettings(CVAR_SPEEDFLIP_TRAINER); 
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Readme"))
		{
			RenderReadmeTab(); 
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void KickoffPractice::RenderSettingsTab()
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

	ImGui::SetNextItemWidth(300);
	if (ImGui::SliderInt("Countdown length", &countdownLength, 1, 5, countdownLength == 1 ? "%d second" : "%d seconds"))
		cvarManager->getCvar(CVAR_COUNTDOWN_LENGTH).setValue(countdownLength);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Countdown length in seconds. Affected by slow-motion.");

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
		ImGui::SetTooltip("Show Speedflip Trainer overlay while training kickoffs.\nSee the \"Speedflip\" tab for more settings.");

	ImGui::Spacing();

	CommandButton("Reset Training/Recording", RESET_COMMAND);
	ImGui::SameLine();
	CommandButton("Repeat Last Command", REPEAT_COMMAND);

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
		activePositions.insert(Utils::allKickoffPositions.begin(), Utils::allKickoffPositions.end());
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

	for (KickoffPosition position : Utils::allKickoffPositions)
	{
		auto command = RECORD_COMMAND + " " + getKickoffArg(position);
		auto label = "Record " + Utils::getKickoffPositionName(position);

		CommandButton(label, command);

		ImGui::SameLine();
	}
	ImGui::Text("(saved automatically)");
	ImGui::Spacing();

	CommandButton("Save Last Attempt", SAVE_COMMAND);
	ImGui::SameLine();
	ImGui::Text("(use while not recording)");

	SpacedSeparator();

	ImGui::Text("Recorded Kickoffs");
	ImGui::Spacing();

	ImGui::Text(
		"Only kickoffs for the current game-mode are shown.\n"
		"Start free-play for the desired game-mode to select kickoffs for training."
	);
	ImGui::Spacing();

	if (ImGui::Button("Reload kickoffs"))
		readKickoffsFromDisk();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Reload recorded kickoffs from files.");

	ImGui::Spacing();

	bool changedActiveKickoffs = false;

	for (KickoffPosition position : Utils::allKickoffPositions)
	{
		auto positionName = Utils::getKickoffPositionName(position);
		ImGui::Text(positionName.c_str());

		auto kickoffs = kickoffLoader->getKickoffs(gameMode, position);

		if (kickoffs.empty())
		{
			ImGui::Text("(no kickoffs recorded)");
			ImGui::Spacing();
			continue;
		}

		for (auto& kickoff : kickoffs)
		{
			ImGui::PushID(kickoff->name.c_str());

			if (kickoff == kickoffLoader->getCurrentKickoff())
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));

			if (ImGui::Checkbox(kickoff->name.c_str(), &kickoff->isActive))
				changedActiveKickoffs = true;

			if (kickoff == kickoffLoader->getCurrentKickoff())
				ImGui::PopStyleColor();

			ImGui::SameLine();
			CommandButton("Replay", REPLAY_COMMAND + " \"" + kickoff->name + "\"");

			ImGui::SameLine();
			if (ImGui::Button("Rename"))
			{
				tempName = kickoff->name;
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
					this->renameKickoffFile(kickoff->name, tempName, [&]()
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
				auto text = std::format("Confirm you want to delete this recording:\n{}", kickoff->name);
				ImGui::Text(text.c_str());
				ImGui::Spacing();
				ImGui::Text("This operation cannot be undone!");
				ImGui::Spacing();

				if (ImGui::Button("Delete", ImVec2(120, 0)))
				{
					this->deleteKickoffFile(kickoff->name, [&]()
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
		kickoffStorage->saveActiveKickoffs(kickoffLoader->getKickoffs());
}

void KickoffPractice::RenderReadmeTab()
{
	if (ImGui::TreeNode("How it works"))
	{
		ImGui::TextWrapped("The plugin works by recording your inputs at every game tick.");
		ImGui::TextWrapped("When you want to practice, the plugin will spawn a bot in freeplay (and teleport you, limit your boost, set up a countdown, etc.) and then, at every tick, replay the pre-recorded inputs with the bot.");
		ImGui::TextWrapped("That's the core of the plugin, the rest is just UI and little features.");

		ImGui::Spacing(); ImGui::TreePop();
	}

	if (ImGui::TreeNode("Troubleshooting"))
	{
		ImGui::TextWrapped("Something now working as expected? Look at the BakkesMod console first (open with `F6`)! There might be some error message telling you what's wrong.");
		ImGui::TextWrapped("If this still does not resolve your issue, don't hestitate to open a issue on GitHub. Please be as specific as possible when describing your issue.");

		ImGui::Spacing(); ImGui::TreePop();
	}

	if (ImGui::TreeNode("Setup and Settings"))
	{
		if (ImGui::TreeNode("Quick Start"))
		{
			ImGui::BulletText("Install [BakkesMod](https://bakkesplugins.com/) (PC only).");
			ImGui::BulletText("Install Plugin through the [BakkesMod website](https://bakkesplugins.com/plugin-search/1/kickoff).");
			ImGui::BulletText("Open Bakkesmod settings (`F2`). Select the \"Plugins\" tab. Select \"Kickoff Practice\" on the left.");
			ImGui::BulletText("Click \"Record Left Corner\" (or any other position you're interested in).");
			ImGui::BulletText("(optional) Reset freeplay (with the default command) to record another attempt.\n_You can always save your last attempt - even when not recording. So just start training against your first recording._");
			ImGui::BulletText("(optional) If you're satisfied with your attempts, uncheck the ones you don't want to train against (or delete them).\n_If you're unsure which recordings are good, use the \"Replay\" feature to check._");
			ImGui::BulletText("Click \"Train Left Corner\" and do another attempt.");
			ImGui::BulletText("Reset freeplay (with the default command) to make another attempt.\n_Congrats! You're training your kickoff._");
			ImGui::BulletText("Click \"Save Last Attempt\" when you had a good kickoff you want to add to your training list.\n_It will automatically be selected for training._");

			ImGui::Spacing(); ImGui::TreePop();
		}

		if (ImGui::TreeNode("Training Different Kickoffs"))
		{
			ImGui::BulletText("If you want to train one position only, use the \"Train _Position_\" buttons.");
			ImGui::BulletText("If you want to select a random kickoff from a set of positions, use the \"Train Selected\" button.");
			ImGui::BulletText("The next kickoff will be randomly selected from all suitable active recordings.\n_If you train \"Right Corner\" and \"Left Corner\", but you have twice the amount of \"Left Corner\" kickoffs selected, they will be two times as likely._");
			ImGui::BulletText("Uncheck a recording, if you want to exclude it from training.\n_Click \"Replay\" next to the recording in question to check it._");

			ImGui::Spacing(); ImGui::TreePop();
		}

		if (ImGui::TreeNode("Manually Adding Recordings"))
		{
			ImGui::BulletText("Go to the BakkesMod data folder (usually `%%appdata%%\\bakkesmod\\bakkesmod`) and open `.\\data\\kickoffpractice`.");
			ImGui::BulletText("You can manually edit these recordings. Just make sure they end in `.kinputs` to be recognized by the plugin.");
			ImGui::BulletText("If you made changes to these files while the game was running, click \"Reload Files\" to apply changes.\n_Renaming a recording file will deselect it._");

			ImGui::Spacing(); ImGui::TreePop();
		}

		if (ImGui::TreeNode("Binding Buttons"))
		{
			ImGui::BulletText("If you want to have custom bindings for different actions, most of them are accessible via commands.");
			ImGui::BulletText("Hover over a button to see what command it uses or explore commands/variables starting with `kickoff_train` in the console (open with F6).\n_You could bind `kickoff_train_auto_restart 1; kickoff_train_active_positions 11000; kickoff_train` to train both corner kickoffs._");
			ImGui::BulletText("I recommend the [Custom Bindings Plugin](https://bakkesplugins.com/plugins/view/228).\n_Example: Bind `kickoff_train 2` (Left Corner) to L3+Left and `kickoff_train 1` (Right Corner) to L3+Right._");

			ImGui::Spacing(); ImGui::TreePop();
		}

		if (ImGui::TreeNode("Slow-motion"))
		{
			ImGui::BulletText("The plugin also works for slower game speeds! So you can record and train in slow-motion.");
			ImGui::BulletText("You have to set the game-speed yourself. There is no automation for that in this plugin.");
			ImGui::BulletText("Slow-motion also affects countdowns. But you can adjust the countdown duration.");

			ImGui::Spacing(); ImGui::TreePop();
		}

		if (ImGui::TreeNode("Fine-Tuning"))
		{
			ImGui::BulletText("Enabling \"Auto-Restart\" will loop the last command indefinitely (Training, Recording or Replaying).\nBut it makes saving the last attempt harder... You have to pause before the countdown is over.\nExit the loop by resetting or exiting freeplay or clicking the \"Reset Training/Recording\" button.");
			ImGui::BulletText("If you don't want the plugin to start when resetting freeplay, uncheck \"Restart on Freeplay Reset\".");
			ImGui::BulletText("\"Time before back to normal\": This settings affects how much time after hitting the ball is still considered a kickoff.\nThis also affects recording length. Changing this setting won't update old recordings (obviously).");
			ImGui::BulletText("\"Show Speedflip Trainer\" will enable the [Speedflip Trainer Plugin](https://bakkesplugins.com/plugins/view/286) overlay. Install the Speedflip Trainer Plugin to actually configure the values in the display.\nI only ported the applicable features of this plugin (for example the automatic game speed adaption got lost). Make sure to check out the original, too.");

			ImGui::Spacing(); ImGui::TreePop();
		}

		ImGui::Spacing(); ImGui::TreePop();
	}


	if (ImGui::TreeNode("Technical Details"))
	{
		ImGui::BulletText("You can use any car or have any control settings you like. You can even change them later on.\nThe plugin will spawn the bot with the right car and settings to re-create the kickoff as good as possible.");
		ImGui::BulletText("Since only the inputs are recorded, there can be slight inconsistencies when replaying it.\nI made sure to test it thoroughly and got it working well, but there might still be issues on other machines.\nIf you have such a case, please let me know!");

		ImGui::Spacing(); ImGui::TreePop();
	}
}

void KickoffPractice::CommandButton(const std::string& label, const std::string& command)
{
	if (ImGui::Button(label.c_str()))
		this->gameWrapper->Execute([this, command](...)
			{
				this->cvarManager->executeCommand(command);
			});
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(command.c_str());
}

void KickoffPractice::SpacedSeparator()
{
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}

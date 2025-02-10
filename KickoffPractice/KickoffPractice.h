#pragma once

#include <fstream>
#include <set>

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"

#include "Common.h"
#include "PersistentStorage.h"
#include "SpeedFlipTrainer.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

static const std::string TRAIN_COMMAND = "kickoff_train";
static const std::string RECORD_COMMAND = "kickoff_train_record";
static const std::string REPLAY_COMMAND = "kickoff_train_replay";
static const std::string SAVE_COMMAND = "kickoff_train_save";

static const std::string CVAR_ENABLED = "kickoff_train_enabled";
static const std::string CVAR_RESTART_ON_RESET = "kickoff_train_restart_on_reset";
static const std::string CVAR_AUTO_RESTART = "kickoff_train_auto_restart";
static const std::string CVAR_SHOW_INDICATOR = "kickoff_train_show_indicator";
static const std::string CVAR_SPEEDFLIP_TRAINER = "kickoff_train_speedflip";
static const std::string CVAR_BACK_TO_NORMAL = "kickoff_train_back_to_normal";
static const std::string CVAR_ACTIVE_POSITIONS = "kickoff_train_active_positions";

struct BoostSettings
{
	int UnlimitedBoostRefCount;
	unsigned long NoBoost;
	float RechargeDelay;
	float RechargeRate;
};

class KickoffPractice : public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase
{
private:
	std::shared_ptr<PersistentStorage> persistentStorage;

	std::unique_ptr<SpeedFlipTrainer> speedFlipTrainer;

	bool pluginEnabled = true;
	bool shouldExecute();
	// Checks `shouldExecute()` after the timeout.
	void setTimeoutChecked(float seconds, std::function<void()> callback);

	// Requires the correct values to be set for each `mode`:
	// - `KickoffMode::Training`: `loadedKickoffs` and (`positionOverride` or `activePositions`) to randomly select from
	// - `KickoffMode::Recording`: `currentKickoffPosition`
	// - `KickoffMode::Replaying`: `currentKickoff` and `currentKickoffPosition`
	// Call it again to repeat the last command.
	void start();
	// Requires `mode`, `currentKickoff` and `currentKickoffPosition` to be set.
	// Also expects that we do not freeze the players in the current physics frame (i.e. during waiting for kickoff).
	// Best to not call it directly and use `start()` instead.
	void setupKickoff();
	// Works while waiting and only for the current kickoff.
	void startCountdown(int seconds, int kickoffCounterAtStart, std::function<void()> onCompleted);
	// Controls bot (and player during replay) by overriding inputs. Also records inputs.
	// Freezes the player in place and sets boost amount while waiting.
	void onVehicleInput(CarWrapper car, ControllerInput* input);
	// Get input for the current tick for current recording.
	std::optional<ControllerInput> getRecordedInput();
	// Reverts everything the plugin did.
	void reset();

	std::vector<RecordedKickoff> loadedKickoffs;
	// Only set through `setCurrentKickoff()`.
	RecordedKickoff* currentKickoff = nullptr;
	void setCurrentKickoff(RecordedKickoff* kickoff);
	// Inputs for the last kickoff regardless of `mode`. Resets the next time the countdown finishes.
	std::vector<ControllerInput> recordedInputs;

	void removeBots();
	void removeBot(CarWrapper car);
	bool isBot(CarWrapper car);

	// Base folder for all files.
	std::filesystem::path configPath;
	// Gets all necessary information and calls `writeRecording()`.
	void saveRecording();
	std::string getNewRecordingName() const;
	// Adds the last attempt to `loadedKickoffs` and saves it to file.
	void writeRecording(RecordedKickoff& kickoff);
	// Writes the names of all active/selected kickoffs to a file.
	void writeActiveKickoffs();
	void readActiveKickoffs();

	void readKickoffFiles();
	RecordedKickoff readKickoffFile(std::filesystem::path filePath);

	// Also renames the recording file.
	void renameKickoff(RecordedKickoff* kickoff, std::string newName, std::function<void()> onSuccess) const;
	// Also deletes the recording file. Make sure `kickoff` points to some element of `loadedKickoffs`.
	void deleteKickoff(RecordedKickoff* kickoff, std::function<void()> onSuccess);

	// To avoid interrupting the freeplay experience for the player...
	void recordBoostSettings();
	void resetBoostSettings();
	static void applyBoostSettings(BoostWrapper boost, BoostSettings settings);
	BoostSettings boostSettings{};

	KickoffMode mode = KickoffMode::Training; // TODO: Link mode to current position/kickoff values to check what's required.
	KickoffState kickoffState = KickoffState::nothing;
	// Often set from `currentKickoff`, but necessary for recording (where the is no current kickoff).
	KickoffPosition currentKickoffPosition = KickoffPosition::BackCenter;

	// Physics frame when the kickoff started, i.e. the countdown ran out.
	int startingFrame = 0;
	// How often did we started a kickoff. Helps identifying kickoffs.
	int kickoffCounter = 0;
	// Set after scoring a goal to prevent execution.
	bool isInGoalReplay = false;
	
	// How long (in seconds) after hitting the ball we end the kickoff.
	float timeAfterBackToNormal = 0.5;
	// Kickoff positions currently selected for training.
	std::set<KickoffPosition> activePositions = {
		KickoffPosition::CornerRight,
		KickoffPosition::CornerLeft,
		KickoffPosition::BackRight,
		KickoffPosition::BackLeft,
		KickoffPosition::BackCenter 
	};
	// Readable serialization of `activePositions`
	std::string getActivePositionsMask();
	void setActivePositionFromMask(std::string mask);
	// Set this to ignore `activePositions` and only train a single kickoff.
	std::optional<KickoffPosition> positionOverride;
	// Hook into "Reset Freeplay" binding.
	bool restartOnTrainingReset = true;
	// Automatically repeat the last command.
	bool autoRestart = false;
	// Shows some information about the current state on-screen.
	bool showIndicator = true;
	void renderIndicator(CanvasWrapper canvas);
	// SpeedFlipTrainer integration, but we use our own state for enabling.
	bool showSpeedFlipTrainer = true;

	// Used for renaming kickoffs in the UI.
	std::string tempName;
	// Execute command from UI, close menu and show command on hover.
	void CommandButton(const std::string& label, const std::string& command);

	// Number 1-5 to `KickoffPosition`.
	static std::optional<KickoffPosition> parseKickoffArg(std::string arg);
public:
	void onLoad() override;
	void onUnload() override;
	void RenderSettings() override;
};

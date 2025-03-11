#pragma once

#include <set>

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"

#include "Common.h"
#include "PersistentStorage.h"
#include "SpeedFlipTrainer.h"
#include "KickoffStorage.h"
#include "KickoffLoader.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

static const std::string TRAIN_COMMAND = "kickoff_train";
static const std::string RECORD_COMMAND = "kickoff_train_record";
static const std::string REPLAY_COMMAND = "kickoff_train_replay";
static const std::string SAVE_COMMAND = "kickoff_train_save";
static const std::string RESET_COMMAND = "kickoff_train_reset";
static const std::string REPEAT_COMMAND = "kickoff_train_repeat";
static const std::string SELECT_COMMAND = "kickoff_train_select";

static const std::string CVAR_ENABLED = "kickoff_train_enabled";
static const std::string CVAR_RESTART_ON_RESET = "kickoff_train_restart_on_reset";
static const std::string CVAR_AUTO_RESTART = "kickoff_train_auto_restart";
static const std::string CVAR_SHOW_INDICATOR = "kickoff_train_show_indicator";
static const std::string CVAR_SPEEDFLIP_TRAINER = "kickoff_train_speedflip";
static const std::string CVAR_CLOSE_SETTINGS = "kickoff_train_close_settings";
static const std::string CVAR_BACK_TO_NORMAL = "kickoff_train_back_to_normal";
static const std::string CVAR_COUNTDOWN_LENGTH = "kickoff_train_countdown_length";
static const std::string CVAR_ACTIVE_POSITIONS = "kickoff_train_active_positions";

class KickoffPractice : public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase
{
private:
	std::shared_ptr<PersistentStorage> persistentStorage;

	std::unique_ptr<SpeedFlipTrainer> speedFlipTrainer;

	std::unique_ptr<KickoffStorage> kickoffStorage;

	std::unique_ptr<KickoffLoader> kickoffLoader;

	void registerCvars();
	void registerCommands();
	void load();
	void unload();
	void hookEvents();
	void unhookEvents();
	void determineGameMode();

	// Are all hooks loaded and files read?
	bool loaded = false;
	// Did the user enable the plugin?
	bool pluginEnabled = true;
	bool shouldExecute(bool log = false);

	// Requires the correct values to be set for each `mode`:
	// - `KickoffMode::Training`: `loadedKickoffs` and (`positionOverride` or `activePositions`) to randomly select from
	// - `KickoffMode::Recording`: `currentKickoffPosition`
	// - `KickoffMode::Replaying`: `currentKickoff` and `currentKickoffPosition`
	// Call it again to repeat the last command.
	void start();
	// Set when everything is ready to update the game state for the next kickoff.
	bool shouldSetupKickoff = false;
	// Did we just spawn a bot? We need to wait until its setup is done for our changes to be effective.
	bool spawnBotCalled = false;
	// Requires `mode`, `currentKickoff` and `currentKickoffPosition` to be set.
	// Also expects that we do not freeze the players in the current physics frame (i.e. during waiting for kickoff).
	void setupKickoff();
	void setupPlayer(CarWrapper player);
	void setupBot(CarWrapper bot);
	// Countdown logic: Aligned with physics frames.
	void initCountdown();
	void doCountdown();
	void updateBall();
	// Gets called once each physics frame before processing inputs.
	void onPhysicsFrame();
	// Controls bot (and player during replay) by overriding inputs. Also records inputs.
	// Freezes the player in place and sets boost amount while waiting.
	void onVehicleInput(CarWrapper car, ControllerInput* input);
	// Get input for the current tick for current recording.
	std::optional<ControllerInput> getRecordedInput();
	// Resets everything the plugin did.
	void reset();
	// Resets only the plugin-internal state.
	void resetPluginState();
	// Resets the external game-state.
	void resetGameState();

	// Updates the current kickoff and the position.
	void setCurrentKickoff(std::shared_ptr<RecordedKickoff> kickoff);

	// Recording for the last kickoff regardless of `mode`. Resets the next time the countdown reaches 0.
	std::shared_ptr<RecordedKickoff> lastAttempt;
	void initNewRecording();
	std::string getNewRecordingName() const;
	void recordInput(ControllerInput& input);
	void saveLastAttempt();
	void readKickoffsFromDisk();
	void renameKickoffFile(std::string oldName, std::string newName, std::function<void()> onSuccess);
	void deleteKickoffFile(std::string name, std::function<void()> onSuccess);

	void removeBots();
	void removeBot(CarWrapper car);
	bool isBot(CarWrapper car);

	// To avoid interrupting the freeplay experience for the player...
	void recordBoostSettings();
	void resetBoostSettings();
	static void applyBoostSettings(BoostWrapper boost, BoostSettings settings);
	BoostSettings boostSettings{};

	std::optional<GameMode> gameMode;
	KickoffMode mode = KickoffMode::Training; // TODO: Link mode to current position/kickoff values to check what's required.
	KickoffState kickoffState = KickoffState::Nothing;
	// Often set from `currentKickoff`, but necessary for recording (where the is no current kickoff).
	KickoffPosition currentKickoffPosition = KickoffPosition::BackCenter;

	// Physics frame when the kickoff started, i.e. the countdown ran out.
	int startingFrame = 0;
	// How often did we started a kickoff. Helps identifying kickoffs.
	int kickoffCounter = 0;
	// Set after scoring a goal to prevent execution. Can be incorrect at the start when loading the plugin after scoring.
	bool isInGoalReplay = false;

	// How long (in seconds) after hitting the ball we end the kickoff.
	float timeAfterBackToNormal = 0.5;
	// Countdown length in seconds. Affected by slow-motion.
	int countdownLength = 3;
	// Kickoff positions currently selected for training.
	std::set<KickoffPosition> activePositions = std::set<KickoffPosition>(Utils::allKickoffPositions.begin(), Utils::allKickoffPositions.end());
	// Readable serialization of `activePositions`
	std::string getActivePositionsMask();
	void setActivePositionFromMask(std::string mask);
	// Set this to ignore `activePositions` and only train a single kickoff.
	std::optional<KickoffPosition> positionOverride;
	// Hook into "Reset Freeplay" binding.
	bool restartOnTrainingReset = true;
	// Automatically repeat the last command.
	bool autoRestart = true;
	// Shows some information about the current state on-screen.
	bool showIndicator = true;
	void renderIndicator(CanvasWrapper canvas);
	// SpeedFlipTrainer integration, but we use our own state for enabling.
	bool showSpeedFlipTrainer = true;
	// Close the BakkesMod settings UI when doing a kickoff.
	bool closeSettingsOnCountdown = true;

	// Used for renaming kickoffs in the UI.
	std::string tempName;
	void RenderSettingsTab();
	void RenderReadmeTab();
	void CommandButton(const std::string& label, const std::string& command);
	void SpacedSeparator();

	// Number 1-5 to `KickoffPosition`.
	static std::optional<KickoffPosition> parseKickoffArg(std::string arg);
	static std::string getKickoffArg(KickoffPosition position);
public:
	void onLoad() override;
	void onUnload() override;
	void RenderSettings() override;
};

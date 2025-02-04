#pragma once

#include <fstream>
#include <set>

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"

#include "PersistentStorage.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

static const std::string TRAIN_COMMAND = "kickoff_train";
static const std::string RECORD_COMMAND = "kickoff_train_record";
static const std::string SAVE_COMMAND = "kickoff_train_save";
static const std::string REPLAY_COMMAND = "kickoff_train_replay";

static const std::string CVAR_ENABLED = "kickoff_train_enabled";
static const std::string CVAR_RESTART_ON_RESET = "kickoff_train_restart_on_reset";
static const std::string CVAR_AUTO_RESTART = "kickoff_train_auto_restart";
static const std::string CVAR_SHOW_INDICATOR = "kickoff_train_show_indicator";
static const std::string CVAR_BACK_TO_NORMAL = "kickoff_train_back_to_normal";
static const std::string CVAR_ACTIVE_POSITIONS = "kickoff_train_active_positions";

enum KickoffPosition
{
	CornerRight = 0,
	CornerLeft = 1,
	BackRight = 2,
	BackLeft = 3,
	BackCenter = 4,
};
enum KickoffSide
{
	Blue = true,
	Orange = false,
};
enum class KickoffState
{
	// Kickoff is over or countdown wasn't started.
	nothing,
	// Countdown is active, not moving.
	waitingToStart,
	// Countdown is over, bot and player moving.
	started
};
enum class KickoffMode
{
	Training,
	Recording,
	Replaying
};

struct RecordedKickoff
{
	std::string name;
	KickoffPosition position = KickoffPosition::CornerLeft;
	int carBody = 23; // Octane
	GamepadSettings settings = GamepadSettings(0, 0.5, 1, 1);
	std::vector<ControllerInput> inputs;
	bool isActive = false;
};

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

	bool pluginEnabled;
	bool shouldExecute();
	void setTimeoutChecked(float seconds, std::function<void()> callback);

	void start();
	void setupKickoff();
	void startCountdown(int seconds, int kickoffCounterAtStart, std::function<void()> onCompleted);
	void onVehicleInput(CarWrapper car, ControllerInput* input);
	std::optional<ControllerInput> getRecordedInput();
	void reset();
	void saveRecording();
	std::string getNewRecordingName() const;

	std::vector<RecordedKickoff> loadedKickoffs; // TODO: Introduce setter to update `currentKickoff` pointer.
	RecordedKickoff* currentKickoff;
	std::vector<ControllerInput> recordedInputs;

	void removeBots();
	void removeBot(CarWrapper car);
	bool isBot(CarWrapper car);

	void writeActiveKickoffs();
	void readActiveKickoffs();
	std::filesystem::path configPath;

	void readKickoffFiles();
	RecordedKickoff readKickoffFile(std::filesystem::path filePath);

	void recordBoostSettings();
	void resetBoostSettings();
	static void applyBoostSettings(BoostWrapper boost, BoostSettings settings);
	BoostSettings boostSettings;

	int startingFrame; // Physics frame when the kickoff started, i.e. the countdown ran out.
	int kickoffCounter; // How often did we start a kickoff this session?
	KickoffPosition currentKickoffPosition;
	KickoffState kickoffState;
	bool botJustSpawned;
	Vector locationBot;
	Rotator rotationBot;
	bool isInGoalReplay;
	KickoffMode mode; // TODO: Link mode to current position/kickoff values to check what's required.

	float timeAfterBackToNormal = 0.5;
	// Kickoff positions currently selected for training.
	std::set<KickoffPosition> activePositions = {
		KickoffPosition::CornerRight,
		KickoffPosition::CornerLeft,
		KickoffPosition::BackRight,
		KickoffPosition::BackLeft,
		KickoffPosition::BackCenter };
	std::string getActivePositionsMask();
	void setActivePositionFromMask(std::string mask);
	// Set this to ignore `activePositions` and only train a single kickoff.
	std::optional<KickoffPosition> positionOverride;
	bool restartOnTrainingReset = true;
	bool autoRestart = false;
	bool showIndicator = true;

	void renderIndicator(CanvasWrapper canvas);

	static Vector getKickoffLocation(int kickoff, KickoffSide side);
	static float getKickoffYaw(int kickoff, KickoffSide side);
	static std::string getKickoffName(int kickoffId);
	static std::optional<KickoffPosition> parseKickoffArg(std::string arg);
public:
	void onLoad() override;
	void onUnload() override;
	void RenderSettings() override;
};

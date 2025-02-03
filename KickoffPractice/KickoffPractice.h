#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
#include <fstream>
#include <set>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

static const std::string TRAIN_COMMAND = "kickoff_train";
static const std::string RECORD_COMMAND = "kickoff_train_record";
static const std::string SAVE_COMMAND = "kickoff_train_save";
static const std::string REPLAY_COMMAND = "kickoff_train_replay";

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
	std::string getRecordingFilename() const;

	std::vector<RecordedKickoff> loadedKickoffs; // TODO: Introduce setter to update `currentKickoff` pointer.
	RecordedKickoff* currentKickoff;
	std::vector<ControllerInput> recordedInputs;
	// A list corresponding to `loadedInputs`.
	// Each item represents which kickoff position is selected for a recording.
	// If it is `0`, the kickoff isn't used in training.
	// If it is between `1` and `5` it is one of the `KickoffPosition`s.
	// Watch out for the `-1` difference!
	std::vector<int> states;

	std::optional<int> getRandomKickoff();
	std::optional<int> getRandomKickoffForPosition(int kickoffId);
	static std::optional<int> getRandomIndex(std::vector<int> vec, std::function<bool(int)> filter);

	void removeBots();
	void removeBot(CarWrapper car);
	bool isBot(CarWrapper car);

	void writeConfigFile();
	void readConfigFile();
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

	static Vector getKickoffLocation(int kickoff, KickoffSide side);
	static float getKickoffYaw(int kickoff, KickoffSide side);
	static std::string getKickoffName(int kickoffId);
	static std::optional<KickoffPosition> parseKickoffArg(std::string arg);
public:
	void onLoad() override;
	void onUnload() override;
	void RenderSettings() override;
};

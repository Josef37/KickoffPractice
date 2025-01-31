#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
#include <fstream>
#include <set>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

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

	void start(std::optional<KickoffPosition> kickoff);
	void startCountdown(int seconds, std::function<void()> onCompleted);
	void onVehicleInput(CarWrapper car, ControllerInput* input);
	void reset();
	void saveRecording();

	std::vector<RecordedKickoff> loadedKickoffs;
	std::vector<ControllerInput> recordedInputs;
	// A list corresponding to `loadedInputs`.
	// Each item represents which kickoff position is selected for a recording.
	// If it is `0`, the kickoff isn't used in training.
	// If it is between `1` and `5` it is one of the `KickoffPosition`s.
	// Watch out for the `-1` difference!
	std::vector<int> states;
	// All currently active kickoff positions.
	std::set<KickoffPosition> loadedKickoffPositions;

	int getRandomKickoffForPosition(int kickoffId);

	void removeBots();
	void removeBot(CarWrapper car);
	bool isBot(CarWrapper car);

	void writeConfigFile();
	void readConfigFile();
	std::filesystem::path configPath;

	void readKickoffFiles();
	RecordedKickoff readKickoffFile(std::filesystem::path filePath);
	void updateLoadedKickoffPositions();

	void recordBoostSettings();
	void resetBoostSettings();
	static void applyBoostSettings(BoostWrapper boost, BoostSettings settings);
	BoostSettings boostSettings;

	int tickCounter;
	KickoffPosition currentKickoffPosition;
	int currentInputIndex;
	KickoffState kickoffState;
	bool botJustSpawned;
	Vector locationBot;
	Rotator rotationBot;
	bool isInReplay;
	bool isRecording;

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

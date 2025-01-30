#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
#include <fstream>

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
	GamepadSettings settings = GamepadSettings();
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

	void start(std::vector<std::string> args);
	void startCountdown(int seconds, std::function<void()> onCompleted);
	void onVehicleInput(CarWrapper car, ControllerInput* input);
	void reset();

	std::vector<int> states; // TODO: Move in to `loadedInputs`.
	std::vector<RecordedKickoff> loadedInputs;
	std::vector<ControllerInput> recordedInputs;

	int getRandomKickoffForId(int kickoffId);
	void removeBots();

	void writeConfigFile();
	void readConfigFile();
	std::filesystem::path configPath;

	void readKickoffFiles();
	RecordedKickoff readKickoffFile(std::string fileName, std::string kickoffName);
	void updateLoadedKickoffIndices();

	void recordBoostSettings();
	void resetBoostSettings();
	static void applyBoostSettings(BoostWrapper boost, BoostSettings settings);
	BoostSettings boostSettings;

	std::vector<int> loadedKickoffIndices;

	int tickCounter;
	int currentKickoffIndex;
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

public:
	void onLoad() override;
	void onUnload() override;
	void RenderSettings() override;
};

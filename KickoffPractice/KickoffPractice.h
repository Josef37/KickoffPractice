#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
#include <fstream>
#include "pathInput.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

enum KickoffPosition {
	CornerRight = 0,
	CornerLeft = 1,
	BackRight = 2,
	BackLeft = 3,
	BackCenter = 4,
};

enum KickoffSide {
	Blue = true,
	Orange = false,
};

enum class KickoffState {
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


class KickoffPractice : public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase
{
private:
	void start(std::vector<std::string> args, GameWrapper* gameWrapper);
	void onVehicleInput(CarWrapper car, ControllerInput* input);
	Vector getKickoffLocation(int kickoff, KickoffSide side);
	float getKickoffYaw(int kickoff, KickoffSide side);
	std::vector<RecordedKickoff> loadedInputs;
	std::vector<int> states;
	std::vector<ControllerInput> recordedInputs;
	int getRandomKickoffForId(int kickoffId);
	void removeBots();
	void storeCarBodies();
	void readConfigFile(std::wstring fileName);
	void writeConfigFile(std::wstring fileName);
	void reset();
	RecordedKickoff readKickoffFile(std::string fileName, std::string kickoffName);
	void recordBoost();
	void loadInputFiles();
	void resetBoost();
	std::string getKickoffName(int kickoffId);
	void updateLoadedKickoffIndices();
	std::vector<int> loadedKickoffIndices;
	int tickCounter;
	int currentKickoffIndex;
	int currentInputIndex;
	KickoffState kickoffState;
	bool botJustSpawned;
	Vector locationBot;
	Rotator rotationBot;
	int unlimitedBoostDefaultSetting;
	bool isInReplay;
	bool isRecording;
	bool pluginEnabled;
	InputPath recordMenu;
	InputPath botMenu;
	float timeAfterBackToNormal = 0.5;
	int botCarID;
	char** carNames;
	std::vector<int> carBodyIDs;
	int nbCarBody = -1;
	int selectedCarUI;

	char botKickoffFolder[128] = "";
	char recordedKickoffFolder[128] = "";
	std::wstring configPath;
public:
	void onLoad();
	void onUnload();
	void RenderSettings() override;
};

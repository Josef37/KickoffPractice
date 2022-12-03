#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
#include <vector>
#include <fstream>
#include "pathInput.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

#define KICKOFF_RIGHT_CORNER 0
#define KICKOFF_LEFT_CORNER 1
#define KICKOFF_BACK_RIGHT 2
#define KICKOFF_BACK_LEFT 3
#define KICKOFF_FAR_BACK_CENTER 4

#define KICKOFF_BLUE_SIDE true
#define KICKOFF_ORANGE_SIDE false

enum class KickoffStates{ nothing,waitingToStart, started};


typedef struct carState
{
	Vector location;
	Vector velocity;
	Rotator rotation;
	Vector angularVelocity;
	float steer;
	float throttle;
	unsigned long handbrakeOn;
	float boostAmount;
	bool isBoosting;
};

typedef struct kickoffStates 
{
	std::string name;
	std::vector<carState> inputs;
};



class kickoffPractice: public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow/*, public BakkesMod::Plugin::PluginWindow*/
{
private:
	void start(std::vector<std::string> args);
	void tick(std::string eventName);
	Vector getKickoffLocation(int kickoff,bool side);
	float getKickoffYaw(int kickoff,bool side);
	kickoffStates currentInputs;
	std::vector<kickoffStates> loadedInputs;
	std::vector<int> states;
	std::vector<carState> recordedInputs;
	int getRandomKickoffForId(int kickoffId);
	void removeBots();
	void storeBodyIndex();
	void readConfigFile(std::wstring fileName);
	void writeConfigFile(std::wstring fileName);
	void reset();
	kickoffStates readKickoffFile(std::string fileName, std::string kickoffName);
	void recordBoost();
	void loadInputFiles();
	void resetBoost();
	std::string getKickoffName(int kickoffId);
	void checkForKickoffDispo();
	std::vector<int> kickoffDispo;
	int tickCompteur;
	int currrentKickoffIndex;
	int currentInputIndex;
	KickoffStates kickoffState;
	bool botJustSpawned;
	Vector locationBot;
	Rotator rotationBot;
	int unlimitedBoostDefaultSetting;
	bool isInReplay;
	bool isRecording;
	bool pluginEnabled;
	InputPath recordMenu;
	InputPath botMenu;
	bool spawnBotDuringRecord;
	float timeAfterBackToNormal = 0.5;
	int botCarID;
	char ** carNames;
	std::vector<int> carBodyIDs;
	int nbCarBody =  -1;
	int selectedCarUI;

	char botKickoffFolder[128] = "";
	char recordedKickoffFolder[128] = "";
	std::wstring configPath;
public:
	virtual void onLoad();
	virtual void onUnload();
	void RenderSettings() override;
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;
	
};

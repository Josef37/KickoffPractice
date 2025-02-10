#pragma once

#include <optional>
#include <memory>

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"

#include "Common.h"

enum class JumpType
{
	None,
	Jump,
	DoubleJump,
	Dodge
};

// Records a kickoff from a replay.
// 
// Since most replays are recorded in 30 FPS (check yourself with `GetReplayFPS()`)
// we are only able to get useful inputs for every 4 ticks.
class ReplayRecorder
{
private:
	std::shared_ptr<GameWrapper> gameWrapper;
	std::shared_ptr<CVarManagerWrapper> cvarManager;

	// Are we currently recording?
	bool isRecording;

	// Where did the recording start?
	RBState startingState;
	// Replay frame (not physics frame). For checking continuity.
	int lastReplayFrame;

	// Recorded inputs for every frame.
	std::vector<ControllerInput> inputs;
	// Which kind of jump input did we have the last frame?
	JumpType lastJump;

	// User preferences
	GamepadSettings settings;
	int carBody;

	// Replay information for naming.
	std::string replayName;
	std::string playerName;

	std::optional<CarWrapper> getViewedCar();

public:
	ReplayRecorder(
		std::shared_ptr<GameWrapper> gameWrapper,
		std::shared_ptr<CVarManagerWrapper> cvarManager
	);

	void startRecording();
	void stopRecording();
	void recordCurrentInput();

	RecordedKickoff getRecording() const;
};
#pragma once

#include <memory>

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/CVarManagerWrapper.h"
#include "bakkesmod/wrappers/wrapperstructs.h"
#include "bakkesmod/wrappers/canvaswrapper.h"

#include "Attempt.h"

using namespace std;

class SpeedFlipTrainer
{
private:
	shared_ptr<GameWrapper> gameWrapper;
	shared_ptr<CVarManagerWrapper> cvarManager;

	// Reuse the cvars from the SpeedFlipTrainer plugin.
	void BindToCvarsFromPlugin();

	// Whether plugin is enabled
	shared_ptr<bool> enabled = make_shared<bool>(true);

	// Whether to show various meters
	shared_ptr<bool> showAngleMeter = make_shared<bool>(true);
	shared_ptr<bool> showPositionMeter = make_shared<bool>(true);
	shared_ptr<bool> showFlipMeter = make_shared<bool>(true);
	shared_ptr<bool> showJumpMeter = make_shared<bool>(true);

	// Optimal left angle for dodge
	shared_ptr<int> optimalLeftAngle = make_shared<int>(-30);

	// Optimal right angle for dodge
	shared_ptr<int> optimalRightAngle = make_shared<int>(30);

	// Physics ticks the flip canceled should be performed under
	shared_ptr<int> flipCancelThreshold = make_shared<int>(13);

	// Physics ticks range during which first jump should be performed
	shared_ptr<int> jumpLow = make_shared<int>(40);
	shared_ptr<int> jumpHigh = make_shared<int>(90);

	int startingPhysicsFrame = -1;
	float startingTime = 0;

	Attempt attempt;

	std::function<bool()> ShouldExecute;

	// Records metrics for the speedflip being performed.
	// Only call for the car you want to measure (i.e. the player).
	void Measure(CarWrapper& car, ControllerInput& input);

	// Render functions to render various meters and measured values on screen
	void RenderAngleMeter(CanvasWrapper canvas, float screenWidth, float screenHeight);
	void RenderFlipCancelMeter(CanvasWrapper canvas, float screenWidth, float screenHeight);
	void RenderFirstJumpMeter(CanvasWrapper canvas, float screenWidth, float screenHeight);
	void RenderPositionMeter(CanvasWrapper canvas, float screenWidth, float screenHeight) const;

public:
	SpeedFlipTrainer(
		shared_ptr<GameWrapper> gameWrapper,
		shared_ptr<CVarManagerWrapper> cvarManager,
		std::function<bool()> shouldExecute
	);

	// Only call for the relevant car (i.e. the player car)
	// and only once you want to start recording.
	void OnVehicleInput(CarWrapper& car, ControllerInput* input);
	// Only call for the relevant car (i.e. the player car).
	// Safe to call multiple times. Only first hit relevant.
	void OnBallHit(CarWrapper& car);
	// Only resets the starting time. The current attempt is only overwritten when starting a new one.
	void Reset();

	// Renders the full plugin overlay.
	void RenderMeters(CanvasWrapper canvas);
};

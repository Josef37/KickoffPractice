#pragma once

#include <memory>

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/CVarManagerWrapper.h"
#include "bakkesmod/wrappers/wrapperstructs.h"
#include "bakkesmod/wrappers/canvaswrapper.h"

#include "Attempt.h"
#include "RenderMeter.h"
#include "PersistentStorage.h"

using namespace std;

static const string CVAR_LEFT_ANGLE = "kickoff_train_sf_left_angle";
static const string CVAR_RIGHT_ANGLE = "kickoff_train_sf_right_angle";
static const string CVAR_CANCEL_THRESHOLD = "kickoff_train_sf_cancel_threshold";
static const string CVAR_SECOND_JUMP_THRESHOLD = "kickoff_train_sf_second_jump_threshold";
static const string CVAR_JUMP_LOW = "kickoff_train_sf_jump_low";
static const string CVAR_JUMP_HIGH = "kickoff_train_sf_jump_high";
static const string CVAR_SHOW_ANGLE = "kickoff_train_sf_show_angle";
static const string CVAR_SHOW_POSITION = "kickoff_train_sf_show_position";
static const string CVAR_SHOW_FIRST_JUMP = "kickoff_train_sf_show_jump";
static const string CVAR_SHOW_SECOND_JUMP = "kickoff_train_sf_show_second_jump";
static const string CVAR_SHOW_FLIP = "kickoff_train_sf_show_flip";

inline constexpr Color RED(float opacity = 1) { return Color{ 255, 50, 50, opacity }; };
inline constexpr Color YELLOW(float opacity = 1) { return Color{ 255, 255, 50, opacity }; };
inline constexpr Color GREEN(float opacity = 1) { return Color{ 50, 255, 50, opacity }; }
inline constexpr Color WHITE(float opacity = 1) { return Color{ 255, 255, 255, opacity }; };
inline constexpr Color BLACK(float opacity = 1) { return Color{ 0, 0, 0, opacity }; };

static const Color BACKGROUND = WHITE(0.4);
static const Line BORDER = { WHITE(), 3 };

class SpeedFlipTrainer
{
private:
	shared_ptr<GameWrapper> gameWrapper;
	shared_ptr<CVarManagerWrapper> cvarManager;

	// Whether plugin is enabled
	shared_ptr<bool> enabled = make_shared<bool>(true);

	// Whether to show various meters
	shared_ptr<bool> showAngleMeter = make_shared<bool>(true);
	shared_ptr<bool> showPositionMeter = make_shared<bool>(true);
	shared_ptr<bool> showFlipMeter = make_shared<bool>(true);
	shared_ptr<bool> showFirstJumpMeter = make_shared<bool>(true);
	shared_ptr<bool> showSecondJumpMeter = make_shared<bool>(true);

	// Optimal left angle for dodge
	shared_ptr<int> optimalLeftAngle = make_shared<int>(-30);

	// Optimal right angle for dodge
	shared_ptr<int> optimalRightAngle = make_shared<int>(30);

	// Milliseconds the flip canceled should be performed under
	shared_ptr<int> flipCancelThresholdMs = make_shared<int>(100);

	// Milliseconds the second jump should be performed within
	shared_ptr<int> secondJumpThresholdMs = make_shared<int>(150);

	// Millisecond range during which first jump should be performed
	shared_ptr<int> jumpLowMs = make_shared<int>(450);
	shared_ptr<int> jumpHighMs = make_shared<int>(600);

	int startingPhysicsFrame = -1;
	float startingTime = 0;

	Attempt attempt;

	function<bool()> ShouldExecute;

	// Records metrics for the speedflip being performed.
	// Only call for the car you want to measure (i.e. the player).
	void Measure(CarWrapper& car, ControllerInput& input);

	// Render functions to render various meters and measured values on screen
	void RenderAngleMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize);
	void RenderFlipCancelMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize);
	void RenderSecondJumpMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize);
	void RenderFirstJumpMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize);
	void RenderPositionMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize);

public:
	SpeedFlipTrainer(
		shared_ptr<GameWrapper> gameWrapper,
		shared_ptr<CVarManagerWrapper> cvarManager,
		function<bool()> shouldExecute
	);
	
	void RegisterCvars(shared_ptr<PersistentStorage> persistentStorage);

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

	// Renders ImGui settings dialog.
	void RenderSettings(string CVAR_ENABLE);
};

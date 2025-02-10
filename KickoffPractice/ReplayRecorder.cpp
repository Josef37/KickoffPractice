#include "pch.h"
#include "ReplayRecorder.h"

ReplayRecorder::ReplayRecorder(
	std::shared_ptr<GameWrapper> gameWrapper,
	std::shared_ptr<CVarManagerWrapper> cvarManager
)
{
	this->gameWrapper = gameWrapper;
	this->cvarManager = cvarManager;

	this->isRecording = false;
	this->lastReplayFrame = 0;
	this->settings = GamepadSettings{ 0, 0.5, 1, 1 };
	this->carBody = 0;
}

void ReplayRecorder::startRecording()
{
	if (!gameWrapper->IsInReplay()) return;

	auto server = gameWrapper->GetGameEventAsReplay();
	if (!server) return;

	auto car_ = getViewedCar();
	if (!car_) return;
	auto& car = car_.value();

	LOG("Starting replay recording...");
	isRecording = true;

	startingState = car.GetRBState();
	lastReplayFrame = server.GetCurrentReplayFrame();

	inputs.clear();
	lastJump = JumpType::None;

	settings = GamepadSettings{};
	settings.ControllerDeadzone = 0; // No way to determine and does not affect how inputs are interpreted.
	settings.DodgeInputThreshold = car.GetPRI().GetDodgeInputThreshold(); // This is 0.5 for every way to get...
	settings.SteeringSensitivity = car.GetPRI().GetSteeringSensitivity();
	settings.AirControlSensitivity = car.GetPRI().GetAirControlSensitivity();

	carBody = car.GetLoadoutBody();

	replayName = server.GetReplay().GetReplayName().ToString();
	playerName = car.GetOwnerName();
}

void ReplayRecorder::stopRecording()
{
	if (!isRecording) return;

	LOG("Stopping replay recording...");
	isRecording = false;
}

void ReplayRecorder::recordCurrentInput()
{
	if (!isRecording) return;
	if (!gameWrapper->IsInReplay()) return;

	auto server = gameWrapper->GetGameEventAsReplay();
	if (!server) return;

	auto car_ = getViewedCar();
	if (!car_) return;
	auto& car = car_.value();

	if (car.GetOwnerName() != playerName)
	{
		// TODO: Could use `car.GetPRI().GetPlayerID()` instead.
		LOG("Viewed car name changed from {} to {}.", playerName, car.GetOwnerName());
		stopRecording();
		return;
	}

	auto currentFrame = server.GetCurrentReplayFrame();
	if (currentFrame < lastReplayFrame || currentFrame > lastReplayFrame + 1)
	{
		LOG("Found jump in replay from frame {} to {}.", lastReplayFrame, currentFrame);
		stopRecording();
		return;
	}
	lastReplayFrame = currentFrame;

	// TEST
	auto boost = car.GetBoostComponent();
	auto pri = car.GetPRI();
	if (!boost) LOG("No boost");
	if (!pri) LOG("No PRI");

	auto jump = car.GetJumpComponent().GetbActive();
	auto doubleJump = car.GetDoubleJumpComponent().GetbActive();
	auto dodge = car.GetDodgeComponent().GetbActive();

	JumpType currentJump = JumpType::None;
	if (jump) currentJump = JumpType::Jump;
	else if (doubleJump) currentJump = JumpType::DoubleJump;
	else if (dodge) currentJump = JumpType::Dodge;

	ControllerInput input;
	input.Throttle = car.GetInput().Throttle;
	input.Steer = car.GetInput().Steer;
	input.Pitch = car.GetInput().Pitch; // TODO: Always 0...
	input.Yaw = car.GetInput().Yaw; // TODO: Always 0...
	input.Roll = car.GetInput().Roll; // TODO: Always 0...
	input.DodgeForward = car.GetDodgeComponent().GetDodgeDirection().X; // TODO: Always 0...
	input.DodgeStrafe = car.GetDodgeComponent().GetDodgeDirection().Y; // TODO: Always 0...
	input.Handbrake = car.GetInput().Handbrake;
	input.Jump = currentJump != JumpType::None; // TODO: Does not release jump...
	input.ActivateBoost = car.IsBoostCheap();
	input.HoldingBoost = car.IsBoostCheap();
	input.Jumped = input.Jump & (currentJump != lastJump);
	
	inputs.push_back(input);

	lastJump = currentJump;
}

RecordedKickoff ReplayRecorder::getRecording() const
{
	auto kickoffPosition = Utils::getKickoffForLocation(startingState.Location);

	std::string timestamp = Utils::getCurrentTimestamp();
	auto kickoffName = Utils::getKickoffPositionName(kickoffPosition);

	RecordedKickoff recording;
	recording.name = timestamp + " " + kickoffName + " " + playerName + " " + replayName;
	recording.isActive = false;
	recording.position = kickoffPosition;
	recording.carBody = carBody;
	recording.settings = settings;
	recording.inputs = inputs;
	return recording;
}

std::optional<CarWrapper> ReplayRecorder::getViewedCar()
{
	if (!gameWrapper->IsInReplay()) return std::nullopt;

	auto replayServer = gameWrapper->GetGameEventAsReplay();
	if (!replayServer) return std::nullopt;

	auto viewTarget = replayServer.GetViewTarget();
	if (!viewTarget) return std::nullopt;

	for (auto car : replayServer.GetCars())
	{
		if (viewTarget.memory_address == car.memory_address)
			return car;
	}
	return std::nullopt;
}

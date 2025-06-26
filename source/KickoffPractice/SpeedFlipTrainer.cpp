#include "pch.h"	
#include "SpeedFlipTrainer.h"

void SpeedFlipTrainer::Measure(CarWrapper& car, ControllerInput& input)
{
	int currentPhysicsFrame = gameWrapper->GetEngine().GetPhysicsFrame();
	int currentTick = currentPhysicsFrame - startingPhysicsFrame;
	attempt.currentTick = currentTick;

	auto movement = car.GetLocation() - attempt.currentLocation;
	attempt.traveledSideways += abs(SidewaysOffset(attempt.kickoffDirection, movement));
	attempt.currentLocation = car.GetLocation();

	if (!attempt.jumped && car.GetbJumped())
	{
		attempt.jumped = true;
		attempt.jumpTick = currentTick;
	}

	if (!attempt.dodged && car.IsDodging())
	{
		attempt.dodged = true;
		attempt.dodgedTick = currentTick;
		auto dodge = car.GetDodgeComponent();
		attempt.dodgeAngle = dodge.IsNull() ? 0 : ComputeDodgeAngle(dodge.GetDodgeDirection());
	}
	if (!attempt.dodged && !car.IsOnGround())
	{
		attempt.dodgeAngle = 0;

		auto dodgeDeadzone = gameWrapper->GetSettings().GetGamepadSettings().DodgeInputThreshold;

		if (abs(input.DodgeForward) + abs(input.DodgeStrafe) + abs(input.Roll) >= dodgeDeadzone)
		{
			Vector dodgeDirection = { input.DodgeForward, input.DodgeStrafe + input.Roll, 0 };
			attempt.dodgeAngle = ComputeDodgeAngle(dodgeDirection);
		}
	}

	if (input.Throttle != 1 && input.ActivateBoost != 1)
		attempt.ticksNotPressingThrottle++;
	if (input.HoldingBoost != 1)
		attempt.ticksNotPressingBoost++;

	if (!attempt.flipCanceled && attempt.dodged && input.DodgeForward < -0.8)
	{
		attempt.flipCanceled = true;
		attempt.flipCancelTick = currentTick;
	}
}

float SpeedFlipTrainer::SidewaysOffset(Vector forwards, Vector movement) const
{
	movement.Z = 0;
	forwards.Z = 0;
	forwards.normalize();
	auto forwardsMovement = forwards * Vector::dot(forwards, movement);
	auto sidewaysMovement = movement - forwardsMovement;
	auto isLeft = Vector::cross(movement, forwards).Z > 0;
	return (isLeft ? -1 : 1) * sidewaysMovement.magnitude();
}

float SpeedFlipTrainer::ComputeDodgeAngle(Vector direction) const
{
	if (direction.X == 0 && direction.Y == 0)
		return 0;

	return atan2(direction.Y, direction.X) * (180 / CONST_PI_D);
}

void SpeedFlipTrainer::OnVehicleInput(CarWrapper& car, ControllerInput* input)
{
	if (!ShouldExecute())
		return;

	if (car.IsNull())
		return;

	auto server = gameWrapper->GetCurrentGameState();
	if (server.IsNull()) return;

	auto ball = server.GetBall();
	if (ball.IsNull()) return;

	if (startingPhysicsFrame < 0)
	{
		startingPhysicsFrame = gameWrapper->GetEngine().GetPhysicsFrame();
		startingTime = gameWrapper->GetEngine().GetPhysicsTime();

		attempt = Attempt();

		attempt.startingLocation = car.GetLocation();
		attempt.currentLocation = car.GetLocation();
		attempt.kickoffDirection = ball.GetLocation() - car.GetLocation();
	}

	if (!attempt.hit())
	{
		Measure(car, *input);
	}
}

void SpeedFlipTrainer::OnBallHit(CarWrapper& car)
{
	if (!ShouldExecute() || attempt.hit())
		return;

	attempt.ticksToBall = car.GetLastBallImpactFrame() - startingPhysicsFrame;
}

void SpeedFlipTrainer::Reset()
{
	if (!ShouldExecute())
		return;

	startingPhysicsFrame = -1;
}

SpeedFlipTrainer::SpeedFlipTrainer(
	shared_ptr<GameWrapper> gameWrapper,
	shared_ptr<CVarManagerWrapper> cvarManager,
	function<bool()> shouldExecute
)
{
	this->gameWrapper = gameWrapper;
	this->cvarManager = cvarManager;
	this->ShouldExecute = shouldExecute;
}

void SpeedFlipTrainer::RegisterCvars(shared_ptr<PersistentStorage> persistentStorage)
{
	persistentStorage->RegisterPersistentCvar(CVAR_TARGET_ANGLE, "30", "Optimal dodge angle", true, true, 0, true, 90, true).bindTo(targetAngle);
	persistentStorage->RegisterPersistentCvar(CVAR_FLIP_CANCEL_THRESHOLD, "100", "Optimal flip cancel threshold.", true, true, 0, true, 500, true).bindTo(flipCancelThresholdMs);
	persistentStorage->RegisterPersistentCvar(CVAR_SECOND_JUMP_THRESHOLD, "150", "Optimal second jump threshold.", true, true, 0, true, 500, true).bindTo(secondJumpThresholdMs);
	persistentStorage->RegisterPersistentCvar(CVAR_JUMP_LOW, "450", "Low threshold for first jump of speedflip.", true, true, 0, true, 1000, false).bindTo(jumpLowMs);
	persistentStorage->RegisterPersistentCvar(CVAR_JUMP_HIGH, "600", "High threshold for first jump of speedflip.", true, true, 0, true, 1000, false).bindTo(jumpHighMs);

	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_ANGLE, "1", "Show angle meter.", true, false, 0, false, 0, true).bindTo(showAngleMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_POSITION, "1", "Show horizontal position meter.", true, false, 0, false, 0, true).bindTo(showPositionMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_FIRST_JUMP, "1", "Show first jump meter.", true, false, 0, false, 0, true).bindTo(showFirstJumpMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_SECOND_JUMP, "1", "Show second jump meter.", true, false, 0, false, 0, true).bindTo(showSecondJumpMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_FLIP_CANCEL, "1", "Show flip cancel meter.", true, false, 0, false, 0, true).bindTo(showFlipCancelMeter);
}

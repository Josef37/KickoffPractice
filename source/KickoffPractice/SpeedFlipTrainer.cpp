#include "pch.h"	
#include "SpeedFlipTrainer.h"

static int ComputeDodgeAngle(Vector direction)
{
	if (direction.X == 0 && direction.Y == 0)
		return 0;

	return atan2f(direction.Y, direction.X) * (180 / CONST_PI_F);
}

static float distance(Vector a, Vector b)
{
	auto delta = a - b;
	delta.Z = 0; // Ignore height difference.
	return delta.magnitude();
}

static float sidewaysOffset(Vector forwards, Vector movement)
{
	movement.Z = 0;
	forwards.Z = 0;
	forwards.normalize();
	auto forwardsMovement = forwards * Vector::dot(forwards, movement);
	auto sidewaysMovement = movement - forwardsMovement;
	auto isLeft = Vector::cross(movement, forwards).Z > 0;
	return (isLeft ? -1 : 1) * sidewaysMovement.magnitude();
}

static int msToTick(float ms)
{
	return lroundf(ms / 1000.f * 120.f);
}

void SpeedFlipTrainer::Measure(CarWrapper& car, ControllerInput& input)
{
	int currentPhysicsFrame = gameWrapper->GetEngine().GetPhysicsFrame();
	int currentTick = currentPhysicsFrame - startingPhysicsFrame;
	attempt.currentTick = currentTick;

	auto movement = car.GetLocation() - attempt.currentLocation;
	attempt.traveledSideways += abs(sidewaysOffset(attempt.kickoffDirection, movement));
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
	// TODO: Preview does not work well with air-roll.
	if (!attempt.dodged && !car.IsOnGround())
	{
		attempt.dodgeAngle = 0;

		auto dodgeDeadzone = gameWrapper->GetSettings().GetGamepadSettings().DodgeInputThreshold;

		if (std::abs(input.DodgeForward) + std::abs(input.DodgeStrafe) >= dodgeDeadzone)
		{
			Vector dodgeDirection = { input.DodgeForward, input.DodgeStrafe, 0 };
			attempt.dodgeAngle = ComputeDodgeAngle(dodgeDirection);
		}
	}

	if (input.Throttle != 1)
		attempt.ticksNotPressingThrottle++;
	if (input.ActivateBoost != 1)
		attempt.ticksNotPressingBoost++;

	if (!attempt.flipCanceled && attempt.dodged && input.DodgeForward < -0.8)
	{
		attempt.flipCanceled = true;
		attempt.flipCancelTick = currentTick;
	}
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
	attempt.timeToBall = startingTime - gameWrapper->GetEngine().GetPhysicsTime();
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
	std::function<bool()> shouldExecute
)
{
	this->gameWrapper = gameWrapper;
	this->cvarManager = cvarManager;
	this->ShouldExecute = shouldExecute;
}

void SpeedFlipTrainer::RegisterCvars(shared_ptr<PersistentStorage> persistentStorage)
{
	persistentStorage->RegisterPersistentCvar(CVAR_LEFT_ANGLE, "-30", "Optimal left angle", true, true, -90, true, 0, true).bindTo(optimalLeftAngle);
	persistentStorage->RegisterPersistentCvar(CVAR_RIGHT_ANGLE, "30", "Optimal right angle", true, true, 0, true, 90, true).bindTo(optimalRightAngle);
	persistentStorage->RegisterPersistentCvar(CVAR_CANCEL_THRESHOLD, "100", "Optimal flip cancel threshold.", true, true, 0, true, 500, true).bindTo(flipCancelThresholdMs);
	persistentStorage->RegisterPersistentCvar(CVAR_JUMP_LOW, "450", "Low threshold for first jump of speedflip.", true, true, 0, true, 1000, false).bindTo(jumpLowMs);
	persistentStorage->RegisterPersistentCvar(CVAR_JUMP_HIGH, "600", "High threshold for first jump of speedflip.", true, true, 0, true, 1000, false).bindTo(jumpHighMs);

	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_ANGLE, "1", "Show angle meter.", true, false, 0, false, 0, true).bindTo(showAngleMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_POSITION, "1", "Show horizontal position meter.", true, false, 0, false, 0, true).bindTo(showPositionMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_JUMP, "1", "Show jump meter.", true, false, 0, false, 0, true).bindTo(showJumpMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_FLIP, "1", "Show flip cancel meter.", true, false, 0, false, 0, true).bindTo(showFlipMeter);
}

void SpeedFlipTrainer::RenderMeters(CanvasWrapper canvas)
{
	if (!ShouldExecute())
		return;

	float SCREENWIDTH = canvas.GetSize().X;
	float SCREENHEIGHT = canvas.GetSize().Y;

	if (*showAngleMeter)
		RenderAngleMeter(canvas, SCREENWIDTH, SCREENHEIGHT);

	if (*showPositionMeter)
		RenderPositionMeter(canvas, SCREENWIDTH, SCREENHEIGHT);

	if (*showFlipMeter)
		RenderFlipCancelMeter(canvas, SCREENWIDTH, SCREENHEIGHT);

	if (*showJumpMeter)
		RenderFirstJumpMeter(canvas, SCREENWIDTH, SCREENHEIGHT);
}

void SpeedFlipTrainer::RenderPositionMeter(CanvasWrapper canvas, float screenWidth, float screenHeight) const
{
	float range = 200;
	float position = sidewaysOffset(attempt.kickoffDirection, attempt.currentLocation - attempt.startingLocation);
	float relLocation = position + range;
	int totalUnits = range * 2;
	float greenRange = 80, yellowRange = 160;

	float opacity = 1;
	Vector2 reqSize = Vector2{ (int)(screenWidth * 70 / 100.f), (int)(screenHeight * 4 / 100.f) };
	int unitWidth = reqSize.X / totalUnits;

	Vector2 boxSize = Vector2{ unitWidth * totalUnits, reqSize.Y };
	Vector2 startPos = Vector2{ (int)((screenWidth / 2) - (boxSize.X / 2)), (int)(screenHeight * 10 / 100.f) };

	struct Color baseColor = { WHITE(opacity) };
	struct Line border = { WHITE(opacity), 2 };

	std::list<MeterRange> ranges;
	if (startingPhysicsFrame > 0)
	{
		if (relLocation >= range - greenRange && relLocation <= range + greenRange)
		{
			ranges.push_back({ GREEN(), range - greenRange, range + greenRange });
		}
		else if (relLocation >= range - yellowRange && relLocation <= range + yellowRange)
		{
			ranges.push_back({ YELLOW(), range - yellowRange, range + yellowRange });
		}
		else
		{
			ranges.push_back({ RED(), 0, (float)totalUnits });
		}
	}

	std::list<MeterMarking> markings;
	markings.push_back({ WHITE(opacity), unitWidth, range - 80 });
	markings.push_back({ WHITE(opacity), unitWidth, range + 80 });
	markings.push_back({ WHITE(opacity), unitWidth, range - 160 });
	markings.push_back({ WHITE(opacity), unitWidth, range + 160 });
	markings.push_back({ BLACK(0.6), unitWidth * 2, relLocation });

	RenderMeter(canvas, startPos, reqSize, baseColor, border, totalUnits, ranges, markings, false);

	int ms = (int)(attempt.ticksNotPressingBoost / 120.0 * 1000.0);
	if (ms != 0)
	{
		canvas.SetColor(255, 255, 50, (char)(255 * opacity));
		//draw time not pressing boost label
		string msg = std::format("Not pressing Boost: {0}ms", ms);
		int width = 200;
		canvas.SetPosition(Vector2{ startPos.X, (int)(startPos.Y + boxSize.Y + 10) });
		canvas.DrawString(msg, 1, 1, true, false);
	}

	ms = (int)(attempt.ticksNotPressingThrottle / 120.0 * 1000.0);
	if (ms != 0)
	{
		canvas.SetColor(255, 255, 50, (char)(255 * opacity));
		//draw time not pressing throttle label
		string msg = std::format("Not pressing Throttle: {0}ms", ms);
		int width = 200;
		canvas.SetPosition(Vector2{ startPos.X, (int)(startPos.Y + boxSize.Y + 25) });
		canvas.DrawString(msg, 1, 1, true, false);
	}
}

void SpeedFlipTrainer::RenderFirstJumpMeter(CanvasWrapper canvas, float screenWidth, float screenHeight)
{
	float opacity = 1.0;

	int redRange = 10;
	int yellowRange = 5;
	int greenRange = msToTick(*jumpHighMs - *jumpLowMs);

	int lowestTick = msToTick(*jumpLowMs) - yellowRange - redRange;
	int totalUnits = greenRange + (2 * yellowRange) + (2 * redRange);

	Vector2 reqSize = Vector2{ (int)(screenWidth * 2 / 100.f), (int)(screenHeight * 56 / 100.f) };
	int unitWidth = reqSize.Y / totalUnits;

	Vector2 boxSize = Vector2{ reqSize.X, unitWidth * totalUnits };
	Vector2 startPos = Vector2{ (int)((screenWidth * 75 / 100.f) + 2.5 * reqSize.X), (int)((screenHeight * 80 / 100.f) - boxSize.Y) };

	struct Color baseColor = WHITE(opacity);
	struct Line border = { WHITE(opacity), 2 };

	float yellowLow = redRange;
	float greenLow = yellowLow + yellowRange;
	float greenHigh = greenLow + greenRange;
	float yellowHigh = greenHigh + yellowRange;

	std::list<MeterMarking> markings;
	markings.push_back({ WHITE(opacity), 3, yellowLow });
	markings.push_back({ WHITE(opacity), 3, greenLow });
	markings.push_back({ WHITE(opacity), 3, greenHigh });
	markings.push_back({ WHITE(opacity), 3, yellowHigh });

	std::list<MeterRange> ranges;
	ranges.push_back({ YELLOW(0.2), yellowLow, greenLow });
	ranges.push_back({ GREEN(0.2), greenLow, greenHigh });
	ranges.push_back({ YELLOW(0.2), greenHigh, yellowHigh });

	int tick = attempt.jumped ? attempt.jumpTick : attempt.currentTick;
	int relativeTick = std::clamp(tick - lowestTick, 0, totalUnits);

	if (attempt.jumped)
	{
		if (relativeTick < yellowLow)
		{
			ranges.push_back({ RED(), 0, yellowLow });
		}
		else if (relativeTick < greenLow)
		{
			ranges.push_back({ YELLOW(), yellowLow, greenLow });
		}
		else if (relativeTick < greenHigh)
		{
			ranges.push_back({ GREEN(), greenLow, greenHigh });
		}
		else if (relativeTick < yellowHigh)
		{
			ranges.push_back({ YELLOW(), greenHigh, yellowHigh });
		}
		else
		{
			ranges.push_back({ RED(), yellowHigh, (float)totalUnits });
		}
	}

	if (lowestTick <= tick && tick <= lowestTick + totalUnits)
	{
		markings.push_back({ BLACK(0.6), unitWidth, (float)relativeTick });
	}

	RenderMeter(canvas, startPos, reqSize, baseColor, border, totalUnits, ranges, markings, true);

	//draw label
	string msg = "First Jump";
	canvas.SetColor(255, 255, 255, 255 * opacity);
	canvas.SetPosition(Vector2{ (int)(startPos.X - 13), (int)(startPos.Y + boxSize.Y + 8) });
	canvas.DrawString(msg, 1, 1, true, false);

	auto ms = (int)(attempt.jumpTick * 1.0 / 120.0 * 1000.0 / 1.0);
	msg = to_string(ms) + " ms";
	canvas.SetPosition(Vector2{ startPos.X, (int)(startPos.Y + boxSize.Y + 8 + 15) });
	canvas.DrawString(msg, 1, 1, true, false);
}

void SpeedFlipTrainer::RenderFlipCancelMeter(CanvasWrapper canvas, float screenWidth, float screenHeight)
{
	float opacity = 1.0;
	int totalUnits = msToTick(*flipCancelThresholdMs);

	Vector2 reqSize = Vector2{ (int)(screenWidth * 2 / 100.f), (int)(screenHeight * 55 / 100.f) };
	int unitWidth = reqSize.Y / totalUnits;

	Vector2 boxSize = Vector2{ reqSize.X, unitWidth * totalUnits };
	Vector2 startPos = Vector2{ (int)(screenWidth * 75 / 100.f), (int)((screenHeight * 80 / 100.f) - boxSize.Y) };

	struct Color baseColor = { WHITE(opacity) };
	struct Line border = { WHITE(opacity), 2 };

	// Let the bar fill up when not cancelled already.
	auto ticks = 0;
	if (attempt.flipCanceled) ticks = attempt.flipCancelTick - attempt.dodgedTick;
	else if (attempt.dodged) ticks = attempt.currentTick - attempt.dodgedTick;

	std::list<MeterRange> ranges;
	std::list<MeterMarking> markings;

	Color meterColor = ticks <= totalUnits * 0.6f
		? GREEN()
		: ticks <= totalUnits * 0.9f
		? YELLOW()
		: RED();
	ranges.push_back({ meterColor, 0, (float)std::clamp(ticks, 0, totalUnits) });

	markings.push_back({ WHITE(opacity), 3, totalUnits * 0.6f });
	markings.push_back({ WHITE(opacity), 3, totalUnits * 0.9f });
	// Don't add a black marker, because the whole bar is usually only about 10 ticks high.

	RenderMeter(canvas, startPos, reqSize, baseColor, border, totalUnits, ranges, markings, true);

	//draw label
	string msg = "Flip Cancel";
	canvas.SetColor(255, 255, 255, (char)(255 * opacity));
	canvas.SetPosition(Vector2{ (int)(startPos.X - 16), (int)(startPos.Y + boxSize.Y + 8) });
	canvas.DrawString(msg, 1, 1, true, false);

	int ms = attempt.flipCanceled ? lroundf(ticks / 120.0 * 1000.0) : 0;
	msg = to_string(ms) + " ms";
	canvas.SetPosition(Vector2{ startPos.X, (int)(startPos.Y + boxSize.Y + 8 + 15) });
	canvas.DrawString(msg, 1, 1, true, false);
}

void SpeedFlipTrainer::RenderAngleMeter(CanvasWrapper canvas, float screenWidth, float screenHeight)
{
	// Cap angle at 90
	int totalUnits = 180;

	float opacity = 1.0;
	Vector2 reqSize = Vector2{ (int)(screenWidth * 66 / 100.f), (int)(screenHeight * 4 / 100.f) };
	int unitWidth = reqSize.X / totalUnits;

	Vector2 boxSize = Vector2{ unitWidth * totalUnits, reqSize.Y };
	Vector2 startPos = Vector2{ (int)((screenWidth / 2) - (boxSize.X / 2)), (int)(screenHeight * 90 / 100.f) };

	struct Color baseColor = { WHITE(opacity) };
	struct Line border = { WHITE(opacity), 2 };

	std::list<MeterRange> ranges;
	std::list<MeterMarking> markings;

	int greenRange = 8, yellowRange = 15;
	float leftTarget = *optimalLeftAngle + 90;
	float rightTarget = *optimalRightAngle + 90;

	float leftYellowLow = leftTarget - yellowRange;
	float leftGreenLow = leftTarget - greenRange;
	float leftGreenHigh = leftTarget + greenRange;
	float leftYellowHigh = leftTarget + yellowRange;

	float rightYellowLow = rightTarget - yellowRange;
	float rightGreenLow = rightTarget - greenRange;
	float rightGreenHigh = rightTarget + greenRange;
	float rightYellowHigh = rightTarget + yellowRange;

	markings.push_back({ WHITE(opacity), 3, leftYellowLow });
	markings.push_back({ WHITE(opacity), 3, leftGreenLow });
	markings.push_back({ WHITE(opacity), 3, leftGreenHigh });
	markings.push_back({ WHITE(opacity), 3, leftYellowHigh });
	markings.push_back({ WHITE(opacity), 3, rightYellowLow });
	markings.push_back({ WHITE(opacity), 3, rightGreenLow });
	markings.push_back({ WHITE(opacity), 3, rightGreenHigh });
	markings.push_back({ WHITE(opacity), 3, rightYellowHigh });

	ranges.push_back({ YELLOW(0.2), leftYellowLow, leftGreenLow });
	ranges.push_back({ GREEN(0.2), leftGreenLow, leftGreenHigh });
	ranges.push_back({ YELLOW(0.2), leftGreenHigh, leftYellowHigh });
	ranges.push_back({ YELLOW(0.2), rightYellowLow, rightGreenLow });
	ranges.push_back({ GREEN(0.2), rightGreenLow, rightGreenHigh });
	ranges.push_back({ YELLOW(0.2), rightGreenHigh, rightYellowHigh });

	float angleAdjusted = 90.f + std::clamp(attempt.dodgeAngle, -90.f, 90.f);

	// Always render the marker, because we compute a "preview" angle before dodging.
	markings.push_back({ BLACK(0.6), unitWidth, angleAdjusted });

	if (attempt.dodged)
	{
		if (angleAdjusted < leftYellowLow)
		{
			ranges.push_back({ RED(), 0, leftYellowLow });
		}
		else if (angleAdjusted < leftGreenLow)
		{
			ranges.push_back({ YELLOW(), leftTarget - yellowRange, leftTarget - greenRange });
		}
		else if (angleAdjusted < leftGreenHigh)
		{
			ranges.push_back({ GREEN(), leftTarget - greenRange, leftTarget + greenRange });
		}
		else if (angleAdjusted < leftYellowHigh)
		{
			ranges.push_back({ YELLOW(), leftTarget + greenRange, leftTarget + yellowRange });
		}
		else if (angleAdjusted < rightYellowLow)
		{
			ranges.push_back({ RED(), leftTarget + yellowRange, rightTarget - yellowRange });
		}
		else if (angleAdjusted < rightGreenLow)
		{
			ranges.push_back({ YELLOW(), rightTarget - yellowRange, rightTarget - greenRange });
		}
		else if (angleAdjusted < rightGreenHigh)
		{
			ranges.push_back({ GREEN(), rightTarget - greenRange, rightTarget + greenRange });
		}
		else if (angleAdjusted < rightYellowHigh)
		{
			ranges.push_back({ YELLOW(), rightTarget + greenRange, rightTarget + yellowRange });
		}
		else
		{
			ranges.push_back({ RED(), rightTarget + yellowRange, (float)totalUnits });
		}
	}

	RenderMeter(canvas, startPos, reqSize, baseColor, border, totalUnits, ranges, markings, false);

	//draw angle label
	canvas.SetColor(255, 255, 255, (char)(255 * opacity));
	canvas.SetPosition(Vector2{ startPos.X, (int)(startPos.Y - 20) });
	auto angleString = std::to_string(!attempt.dodged ? 0 : std::lroundf(attempt.dodgeAngle));
	canvas.DrawString("Dodge Angle: " + angleString + " DEG", 1, 1, true, false);

	//draw time to ball label
	if (attempt.hit() && attempt.ticksToBall > 0)
	{
		auto ms = attempt.ticksToBall * 1.0 / 120.0;
		string msg = std::format("Time to ball: {0:.4f}s", ms);

		int width = (msg.length() * 8) - (5 * 3); // 8 pixels per char - 5 pixels per space

		canvas.SetColor(255, 255, 255, (char)(255 * opacity));
		canvas.SetPosition(Vector2{ startPos.X + (int)(boxSize.X / 2) - (width / 2), startPos.Y - 20 });
		canvas.DrawString(msg, 1, 1, true, false);
	}

	//draw sideways travel label
	string msg = std::format("Horizontal distance traveled: {0:.1f}", attempt.traveledSideways);
	int width = (msg.length() * 6.6);

	if (attempt.traveledSideways < 225)
		canvas.SetColor(50, 255, 50, (char)(255 * opacity));
	else if (attempt.traveledSideways < 425)
		canvas.SetColor(255, 255, 50, (char)(255 * opacity));
	else
		canvas.SetColor(255, 10, 10, (char)(255 * opacity));

	canvas.SetPosition(Vector2{ startPos.X + boxSize.X - width, (int)(startPos.Y - 20) });
	canvas.DrawString(msg, 1, 1, true, false);
}

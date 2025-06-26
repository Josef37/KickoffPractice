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

static int msToTicks(float ms)
{
	return lroundf(ms / 1000.f * 120.f);
}
static float ticksToMs(int tick)
{
	return tick / 120.f * 1000.f;
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

		if (abs(input.DodgeForward) + abs(input.DodgeStrafe) >= dodgeDeadzone)
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
	function<bool()> shouldExecute
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
	persistentStorage->RegisterPersistentCvar(CVAR_SECOND_JUMP_THRESHOLD, "150", "Optimal second jump threshold.", true, true, 0, true, 500, true).bindTo(secondJumpThresholdMs);
	persistentStorage->RegisterPersistentCvar(CVAR_JUMP_LOW, "450", "Low threshold for first jump of speedflip.", true, true, 0, true, 1000, false).bindTo(jumpLowMs);
	persistentStorage->RegisterPersistentCvar(CVAR_JUMP_HIGH, "600", "High threshold for first jump of speedflip.", true, true, 0, true, 1000, false).bindTo(jumpHighMs);

	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_ANGLE, "1", "Show angle meter.", true, false, 0, false, 0, true).bindTo(showAngleMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_POSITION, "1", "Show horizontal position meter.", true, false, 0, false, 0, true).bindTo(showPositionMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_FIRST_JUMP, "1", "Show first jump meter.", true, false, 0, false, 0, true).bindTo(showFirstJumpMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_SECOND_JUMP, "1", "Show second jump meter.", true, false, 0, false, 0, true).bindTo(showSecondJumpMeter);
	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_FLIP, "1", "Show flip cancel meter.", true, false, 0, false, 0, true).bindTo(showFlipMeter);
}

void SpeedFlipTrainer::RenderMeters(CanvasWrapper canvas)
{
	if (!ShouldExecute())
		return;

	float screenWidth = canvas.GetSize().X;
	float screenHeight = canvas.GetSize().Y;

	Vector2F horizontalMeterSize = Vector2F{ screenWidth * 0.66f, screenHeight * 0.04f };

	if (*showAngleMeter)
	{
		Vector2F boxSize = horizontalMeterSize;
		Vector2F startPos = Vector2F{ (screenWidth - boxSize.X) / 2.f, screenHeight * 0.90f };
		RenderAngleMeter(canvas, startPos, boxSize);
	}
	if (*showPositionMeter)
	{
		Vector2F boxSize = horizontalMeterSize;
		Vector2F startPos = Vector2F{ (screenWidth - boxSize.X) / 2.f, screenHeight * 0.10f };
		RenderPositionMeter(canvas, startPos, boxSize);
	}

	Vector2F verticalMeterSize = { screenWidth * 0.02f, screenHeight * 0.55f };
	Vector2F startPos = Vector2F{ screenWidth * 0.80f, (screenHeight * 0.80f) - verticalMeterSize.Y };
	float offset = 2.2f;

	if (*showFirstJumpMeter)
	{
		RenderFirstJumpMeter(canvas, startPos, verticalMeterSize);
		startPos.X -= verticalMeterSize.X * offset;
	}
	if (*showSecondJumpMeter)
	{
		RenderSecondJumpMeter(canvas, startPos, verticalMeterSize);
		startPos.X -= verticalMeterSize.X * offset;
	}
	if (*showFlipMeter)
	{
		RenderFlipCancelMeter(canvas, startPos, verticalMeterSize);
		startPos.X -= verticalMeterSize.X * offset;
	}
}

static void DrawStringCentered(CanvasWrapper canvas, vector<string> lines, float width, float scale = 1)
{
	auto position = canvas.GetPositionFloat();

	for (auto& line : lines)
	{
		auto size = canvas.GetStringSize(line, scale, scale);
		auto offset = (width - size.X) / 2;

		canvas.SetPosition(position + Vector2F(offset, 0));
		canvas.DrawString(line, scale, scale, true);

		position += Vector2F(0, size.Y);
	}
}
static void DrawStringRight(CanvasWrapper canvas, string text, float scale = 1)
{
	auto position = canvas.GetPositionFloat();
	auto size = canvas.GetStringSize(text, scale, scale);

	canvas.SetPosition(position - Vector2F(size.X, 0));
	canvas.DrawString(text, scale, scale, true);
}

void SpeedFlipTrainer::RenderPositionMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize)
{
	float range = 200;
	float position = sidewaysOffset(attempt.kickoffDirection, attempt.currentLocation - attempt.startingLocation);
	float relLocation = position + range;
	float totalUnits = range * 2;
	float unitWidth = boxSize.X / totalUnits;
	float greenRange = 80, yellowRange = 160;

	list<MeterRange> ranges;
	list<MeterMarking> markings;

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
			ranges.push_back({ RED(), 0, totalUnits });
		}
	}

	markings.push_back({ WHITE(), BORDER.width, range - 80 });
	markings.push_back({ WHITE(), BORDER.width, range + 80 });
	markings.push_back({ WHITE(), BORDER.width, range - 160 });
	markings.push_back({ WHITE(), BORDER.width, range + 160 });
	markings.push_back({ BLACK(0.6), unitWidth * 2, relLocation });

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, totalUnits, ranges, markings, false);

	//draw time not pressing boost label
	int ms = ticksToMs(attempt.ticksNotPressingBoost);
	if (ms != 0)
	{
		canvas.SetColor(255, 255, 50, 255);
		string msg = format("Not pressing Boost: {0}ms", ms);
		int width = 200;
		canvas.SetPosition(Vector2F{ startPos.X, startPos.Y + boxSize.Y + 10 });
		canvas.DrawString(msg, 1, 1, true);
	}

	//draw time not pressing throttle label
	ms = ticksToMs(attempt.ticksNotPressingThrottle);
	if (ms != 0)
	{
		canvas.SetColor(255, 255, 50, 255);
		string msg = format("Not pressing Throttle: {0}ms", ms);
		int width = 200;
		canvas.SetPosition(Vector2F{ startPos.X, startPos.Y + boxSize.Y + 25 });
		canvas.DrawString(msg, 1, 1, true);
	}

	//draw sideways travel label
	string msg = format("Horizontal distance traveled: {0:.1f}", attempt.traveledSideways);

	if (attempt.traveledSideways < 225)
		canvas.SetColor(50, 255, 50, 255);
	else if (attempt.traveledSideways < 425)
		canvas.SetColor(255, 255, 50, 255);
	else
		canvas.SetColor(255, 10, 10, 255);

	canvas.SetPosition(Vector2F{ startPos.X + boxSize.X, startPos.Y + boxSize.Y + 10 });
	DrawStringRight(canvas, msg);
}

void SpeedFlipTrainer::RenderFirstJumpMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize)
{
	int redRange = 10;
	int yellowRange = 5;
	int greenRange = msToTicks(*jumpHighMs - *jumpLowMs);

	int lowestTick = msToTicks(*jumpLowMs) - yellowRange - redRange;
	float totalUnits = greenRange + (2 * yellowRange) + (2 * redRange);
	float unitWidth = boxSize.Y / totalUnits;

	float yellowLow = redRange;
	float greenLow = yellowLow + yellowRange;
	float greenHigh = greenLow + greenRange;
	float yellowHigh = greenHigh + yellowRange;

	list<MeterMarking> markings;
	markings.push_back({ WHITE(), BORDER.width, yellowLow });
	markings.push_back({ WHITE(), BORDER.width, greenLow });
	markings.push_back({ WHITE(), BORDER.width, greenHigh });
	markings.push_back({ WHITE(), BORDER.width, yellowHigh });

	list<MeterRange> ranges;
	ranges.push_back({ YELLOW(0.2), yellowLow, greenLow });
	ranges.push_back({ GREEN(0.2), greenLow, greenHigh });
	ranges.push_back({ YELLOW(0.2), greenHigh, yellowHigh });

	int tick = attempt.jumped ? attempt.jumpTick : attempt.currentTick;
	int relativeTick = clamp(tick - lowestTick, 0, (int)totalUnits);

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
			ranges.push_back({ RED(), yellowHigh, totalUnits });
		}
	}

	if (lowestTick <= tick && tick <= lowestTick + totalUnits)
	{
		markings.push_back({ BLACK(0.6), unitWidth, (float)relativeTick });
	}

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, totalUnits, ranges, markings, true);

	//draw label
	float margin = 8;
	vector<string> text = {
		"First Jump",
		to_string(lroundf(ticksToMs(attempt.jumpTick))) + " ms"
	};
	canvas.SetColor(255, 255, 255, 255);
	canvas.SetPosition(Vector2F{ startPos.X, (startPos.Y + boxSize.Y + margin) });
	DrawStringCentered(canvas, text, boxSize.X);
}

void SpeedFlipTrainer::RenderSecondJumpMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize)
{
	float totalUnits = max(1, msToTicks(*secondJumpThresholdMs));
	float unitWidth = boxSize.Y / totalUnits;

	// Let the bar fill up when not dodged already.
	auto ticks = 0;
	if (attempt.dodged) ticks = attempt.dodgedTick - attempt.jumpTick;
	else if (attempt.jumped) ticks = attempt.currentTick - attempt.jumpTick;

	list<MeterRange> ranges;
	list<MeterMarking> markings;

	struct Color meterColor = ticks <= totalUnits * 0.6f
		? GREEN()
		: ticks <= totalUnits * 0.9f
		? YELLOW()
		: RED();
	ranges.push_back({ meterColor, 0, (float)clamp(ticks, 0, (int)totalUnits) });

	markings.push_back({ WHITE(), BORDER.width, totalUnits * 0.6f });
	markings.push_back({ WHITE(), BORDER.width, totalUnits * 0.9f });
	// Don't add a black marker, because the whole bar is usually only about 10 ticks high.

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, totalUnits, ranges, markings, true);

	//draw label
	float margin = 8;
	vector<string> text = {
		"Second Jump",
		to_string(attempt.dodged ? lroundf(ticksToMs(ticks)) : 0) + " ms"
	};
	canvas.SetColor(255, 255, 255, 255);
	canvas.SetPosition(Vector2F{ startPos.X, (startPos.Y + boxSize.Y + margin) });
	DrawStringCentered(canvas, text, boxSize.X);
}

void SpeedFlipTrainer::RenderFlipCancelMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize)
{
	float totalUnits = max(1, msToTicks(*flipCancelThresholdMs));
	float unitWidth = boxSize.Y / totalUnits;

	// Let the bar fill up when not cancelled already.
	auto ticks = 0;
	if (attempt.flipCanceled) ticks = attempt.flipCancelTick - attempt.dodgedTick;
	else if (attempt.dodged) ticks = attempt.currentTick - attempt.dodgedTick;

	list<MeterRange> ranges;
	list<MeterMarking> markings;

	Color meterColor = ticks <= totalUnits * 0.6f
		? GREEN()
		: ticks <= totalUnits * 0.9f
		? YELLOW()
		: RED();
	ranges.push_back({ meterColor, 0, (float)clamp(ticks, 0, (int)totalUnits) });

	markings.push_back({ WHITE(), BORDER.width, totalUnits * 0.6f });
	markings.push_back({ WHITE(), BORDER.width, totalUnits * 0.9f });
	// Don't add a black marker, because the whole bar is usually only about 10 ticks high.

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, totalUnits, ranges, markings, true);

	//draw label
	float margin = 8;
	vector<string> text = {
		"Flip Cancel",
		to_string(attempt.flipCanceled ? lroundf(ticksToMs(ticks)) : 0) + " ms"
	};
	canvas.SetColor(255, 255, 255, 255);
	canvas.SetPosition(Vector2F{ startPos.X, (startPos.Y + boxSize.Y + margin) });
	DrawStringCentered(canvas, text, boxSize.X);
}

void SpeedFlipTrainer::RenderAngleMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize)
{
	// Cap angle at 90
	float totalUnits = 180;
	float unitWidth = boxSize.X / totalUnits;

	list<MeterRange> ranges;
	list<MeterMarking> markings;

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

	markings.push_back({ WHITE(), BORDER.width, leftYellowLow });
	markings.push_back({ WHITE(), BORDER.width, leftGreenLow });
	markings.push_back({ WHITE(), BORDER.width, leftGreenHigh });
	markings.push_back({ WHITE(), BORDER.width, leftYellowHigh });
	markings.push_back({ WHITE(), BORDER.width, rightYellowLow });
	markings.push_back({ WHITE(), BORDER.width, rightGreenLow });
	markings.push_back({ WHITE(), BORDER.width, rightGreenHigh });
	markings.push_back({ WHITE(), BORDER.width, rightYellowHigh });

	ranges.push_back({ YELLOW(0.2), leftYellowLow, leftGreenLow });
	ranges.push_back({ GREEN(0.2), leftGreenLow, leftGreenHigh });
	ranges.push_back({ YELLOW(0.2), leftGreenHigh, leftYellowHigh });
	ranges.push_back({ YELLOW(0.2), rightYellowLow, rightGreenLow });
	ranges.push_back({ GREEN(0.2), rightGreenLow, rightGreenHigh });
	ranges.push_back({ YELLOW(0.2), rightGreenHigh, rightYellowHigh });

	float angleAdjusted = 90.f + clamp(attempt.dodgeAngle, -90.f, 90.f);

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
			ranges.push_back({ RED(), rightTarget + yellowRange, totalUnits });
		}
	}

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, totalUnits, ranges, markings, false);

	//draw angle label
	canvas.SetColor(255, 255, 255, 255);
	canvas.SetPosition(Vector2F{ startPos.X, (startPos.Y - 20) });
	auto angleString = to_string(!attempt.dodged ? 0 : lroundf(attempt.dodgeAngle));
	canvas.DrawString("Dodge Angle: " + angleString + " DEG", 1, 1, true, false);

	//draw time to ball label
	if (attempt.hit() && attempt.ticksToBall > 0)
	{
		auto seconds = ticksToMs(attempt.ticksToBall) / 1000.f;
		string msg = format("Time to ball: {0:.4f}s", seconds);

		canvas.SetColor(255, 255, 255, 255);
		canvas.SetPosition(Vector2F{ startPos.X, startPos.Y - 20 });
		DrawStringCentered(canvas, { msg }, boxSize.X);
	}
}

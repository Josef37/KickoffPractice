#include "pch.h"	
#include "SpeedFlipTrainer.h"
#include <array>

static int ComputeDodgeAngle(DodgeComponentWrapper dodge)
{
	if (dodge.IsNull())
		return 0;

	Vector dd = dodge.GetDodgeDirection();
	if (dd.X == 0 && dd.Y == 0)
		return 0;

	return std::lroundf(atan2f(dd.Y, dd.X) * (180 / CONST_PI_F));
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
		if (!dodge.IsNull())
			attempt.dodgeAngle = ComputeDodgeAngle(dodge);
	}

	if (input.Throttle != 1)
		attempt.ticksNotPressingThrottle++;
	if (input.ActivateBoost != 1)
		attempt.ticksNotPressingBoost++;

	if (!attempt.flipCanceled && attempt.dodged && input.Pitch > 0.8)
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
	int range = 200;
	float position = sidewaysOffset(attempt.kickoffDirection, attempt.currentLocation - attempt.startingLocation);
	int relLocation = lroundf(position) + range;
	int totalUnits = range * 2;

	float opacity = 1.0;
	Vector2 reqSize = Vector2{ (int)(screenWidth * 70 / 100.f), (int)(screenHeight * 4 / 100.f) };
	int unitWidth = reqSize.X / totalUnits;

	Vector2 boxSize = Vector2{ unitWidth * totalUnits, reqSize.Y };
	Vector2 startPos = Vector2{ (int)((screenWidth / 2) - (boxSize.X / 2)), (int)(screenHeight * 10 / 100.f) };

	struct Color baseColor = { WHITE(opacity) };
	struct Line border = { WHITE(opacity), 2 };

	std::list<MeterRange> ranges;
	if (startingPhysicsFrame > 0)
	{
		if (relLocation >= range - 80 && relLocation <= range + 80)
		{
			ranges.push_back({ GREEN(), range - 80, range + 80 });
		}
		else if (relLocation >= range - 160 && relLocation <= range + 160)
		{
			ranges.push_back({ YELLOW(), range - 160, range + 160 });
			ranges.push_back({ YELLOW(), range - 160, range + 160 });
		}
		else
		{
			ranges.push_back({ RED(), 0, totalUnits });
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

	int yellowLow = redRange;
	int greenLow = yellowLow + yellowRange;
	int greenHigh = greenLow + greenRange;
	int yellowHigh = greenHigh + yellowRange;

	std::list<MeterMarking> markings;
	markings.push_back({ WHITE(opacity), 3, yellowLow });
	markings.push_back({ WHITE(opacity), 3, greenLow });
	markings.push_back({ WHITE(opacity), 3, greenHigh });
	markings.push_back({ WHITE(opacity), 3, yellowHigh });

	std::list<MeterRange> ranges;
	ranges.push_back({ YELLOW(0.2), yellowLow, greenLow });
	ranges.push_back({ GREEN(0.2), greenLow, greenHigh });
	ranges.push_back({ YELLOW(0.2), greenHigh, yellowHigh });

	if (attempt.jumped)
	{
		int ticks = clamp(attempt.jumpTick - lowestTick, 0, totalUnits);

		if (ticks < yellowLow)
		{
			ranges.push_back({ RED(), 0, yellowLow });
		}
		else if (ticks < greenLow)
		{
			ranges.push_back({ YELLOW(), yellowLow, greenLow });
		}
		else if (ticks < greenHigh)
		{
			ranges.push_back({ GREEN(), greenLow, greenHigh });
		}
		else if (ticks < yellowHigh)
		{
			ranges.push_back({ YELLOW(), greenHigh, yellowHigh });
		}
		else
		{
			ranges.push_back({ RED(), yellowHigh, totalUnits });
		}

		markings.push_back({ BLACK(0.6), reqSize.Y / totalUnits, ticks });
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

	auto tickBeforeCancel = attempt.flipCancelTick - attempt.dodgedTick;

	// flip cancel time range
	std::list<MeterRange> ranges;
	if (attempt.flipCanceled)
	{
		auto ticks = tickBeforeCancel > totalUnits ? totalUnits : tickBeforeCancel;

		struct Color meterColor;
		if (ticks <= (int)(totalUnits * 0.6f))
			meterColor = GREEN(0.7);
		else if (ticks <= (int)(totalUnits * 0.9f))
			meterColor = YELLOW(0.7);
		else
			meterColor = RED(0.7);

		ranges.push_back({ meterColor.red, meterColor.green, meterColor.blue, 1, 0, ticks });
	}

	std::list<MeterMarking> markings;
	markings.push_back({ WHITE(opacity), 3, ((int)(totalUnits * 0.6f)) });
	markings.push_back({ WHITE(opacity), 3, ((int)(totalUnits * 0.9f)) });
	//markings.push_back({ BLACK(0.6), 10, ticks });

	RenderMeter(canvas, startPos, reqSize, baseColor, border, totalUnits, ranges, markings, true);

	//draw label
	string msg = "Flip Cancel";
	canvas.SetColor(255, 255, 255, (char)(255 * opacity));
	canvas.SetPosition(Vector2{ (int)(startPos.X - 16), (int)(startPos.Y + boxSize.Y + 8) });
	canvas.DrawString(msg, 1, 1, true, false);

	int ms = attempt.flipCanceled ? lroundf(tickBeforeCancel / 120.0 * 1000.0) : 0;
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
	int lTarget = *optimalLeftAngle + 90;
	int rTarget = *optimalRightAngle + 90;

	int lyl = lTarget - yellowRange;
	int lgl = lTarget - greenRange;
	int lgh = lTarget + greenRange;
	int lyh = lTarget + yellowRange;

	int ryl = rTarget - yellowRange;
	int rgl = rTarget - greenRange;
	int rgh = rTarget + greenRange;
	int ryh = rTarget + yellowRange;

	markings.push_back({ WHITE(opacity), 3, lyl });
	markings.push_back({ WHITE(opacity), 3, lgl });
	markings.push_back({ WHITE(opacity), 3, lgh });
	markings.push_back({ WHITE(opacity), 3, lyh });
	markings.push_back({ WHITE(opacity), 3, ryl });
	markings.push_back({ WHITE(opacity), 3, rgl });
	markings.push_back({ WHITE(opacity), 3, rgh });
	markings.push_back({ WHITE(opacity), 3, ryh });

	ranges.push_back({ YELLOW(0.2), lyl, lgl });
	ranges.push_back({ GREEN(0.2), lgl, lgh });
	ranges.push_back({ YELLOW(0.2), lgh, lyh });
	ranges.push_back({ YELLOW(0.2), ryl, rgl });
	ranges.push_back({ GREEN(0.2), rgl, rgh });
	ranges.push_back({ YELLOW(0.2), rgh, ryh });

	if (attempt.dodged)
	{
		int angle = attempt.dodgeAngle;
		if (angle > 90) angle = 90;
		if (angle < -90) angle = -90;

		int angleAdjusted = 90 + angle;
		markings.push_back({ BLACK(0.6), unitWidth, angleAdjusted });

		if (angleAdjusted < lyl)
		{
			ranges.push_back({ RED(), 0, lyl });
		}
		else if (angleAdjusted < lgl)
		{
			ranges.push_back({ YELLOW(), lTarget - yellowRange, lTarget - greenRange });
		}
		else if (angleAdjusted < lgh)
		{
			ranges.push_back({ GREEN(), lTarget - greenRange, lTarget + greenRange });
		}
		else if (angleAdjusted < lyh)
		{
			ranges.push_back({ YELLOW(), lTarget + greenRange, lTarget + yellowRange });
		}
		else if (angleAdjusted < ryl)
		{
			ranges.push_back({ RED(), lTarget + yellowRange, rTarget - yellowRange });
		}
		else if (angleAdjusted < rgl)
		{
			ranges.push_back({ YELLOW(), rTarget - yellowRange, rTarget - greenRange });
		}
		else if (angleAdjusted < rgh)
		{
			ranges.push_back({ GREEN(), rTarget - greenRange, rTarget + greenRange });
		}
		else if (angleAdjusted < ryh)
		{
			ranges.push_back({ YELLOW(), rTarget + greenRange, rTarget + yellowRange });
		}
		else
		{
			ranges.push_back({ RED(), rTarget + yellowRange, totalUnits });
		}
	}

	RenderMeter(canvas, startPos, reqSize, baseColor, border, totalUnits, ranges, markings, false);

	//draw angle label
	canvas.SetColor(255, 255, 255, (char)(255 * opacity));
	canvas.SetPosition(Vector2{ startPos.X, (int)(startPos.Y - 20) });
	canvas.DrawString("Dodge Angle: " + to_string(attempt.dodgeAngle) + " DEG", 1, 1, true, false);

	//draw time to ball label
	if (attempt.hit() && attempt.ticksToBall > 0)
	{
		auto ms = attempt.ticksToBall * 1.0 / 120.0;
		//string msg = std::format("Time to ball: {0:.3f}s", attempt.timeToBall);
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

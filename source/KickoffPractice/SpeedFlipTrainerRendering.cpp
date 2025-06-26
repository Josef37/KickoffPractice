#include "pch.h"	
#include "SpeedFlipTrainer.h"

static int msToTicks(float ms)
{
	return lroundf(ms / 1000.f * 120.f);
}
static float ticksToMs(int tick)
{
	return tick / 120.f * 1000.f;
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
	if (*showFlipCancelMeter)
	{
		RenderFlipCancelMeter(canvas, startPos, verticalMeterSize);
		startPos.X -= verticalMeterSize.X * offset;
	}
}

void SpeedFlipTrainer::RenderPositionMeter(CanvasWrapper canvas, Vector2F startPos, Vector2F boxSize)
{
	float greenRange = 80;
	float yellowRange = 160;
	float totalRange = 200;

	float position = SidewaysOffset(attempt.kickoffDirection, attempt.currentLocation - attempt.startingLocation);
	Bounds bounds = { -totalRange, totalRange };

	list<MeterRange> ranges;
	list<MeterMarking> markings;

	if (startingPhysicsFrame > 0)
	{
		if (position >= -greenRange && position <= greenRange)
		{
			ranges.push_back({ GREEN(), -greenRange, greenRange });
		}
		else if (position >= -yellowRange && position <= yellowRange)
		{
			ranges.push_back({ YELLOW(), -yellowRange, yellowRange });
		}
		else
		{
			ranges.push_back({ RED(), bounds.low, bounds.high });
		}
	}

	markings.push_back({ WHITE(), BORDER.width, -greenRange });
	markings.push_back({ WHITE(), BORDER.width, greenRange });
	markings.push_back({ WHITE(), BORDER.width, -yellowRange });
	markings.push_back({ WHITE(), BORDER.width, yellowRange });
	markings.push_back({ MARKER, position });

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, bounds, ranges, markings, false);

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
	int greenRange = msToTicks(*jumpHighMs) - msToTicks(*jumpLowMs);

	int lowestTick = msToTicks(*jumpLowMs) - yellowRange - redRange;
	float totalUnits = greenRange + (2 * yellowRange) + (2 * redRange);
	Bounds bounds = { lowestTick, lowestTick + totalUnits };

	float greenLow = msToTicks(*jumpLowMs);
	float greenHigh = msToTicks(*jumpHighMs);
	float yellowLow = greenLow - yellowRange;
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

	markings.push_back({ MARKER, (float)tick });

	if (attempt.jumped)
	{
		if (tick < yellowLow)
		{
			ranges.push_back({ RED(), bounds.low, yellowLow });
		}
		else if (tick < greenLow)
		{
			ranges.push_back({ YELLOW(), yellowLow, greenLow });
		}
		else if (tick < greenHigh)
		{
			ranges.push_back({ GREEN(), greenLow, greenHigh });
		}
		else if (tick < yellowHigh)
		{
			ranges.push_back({ YELLOW(), greenHigh, yellowHigh });
		}
		else
		{
			ranges.push_back({ RED(), yellowHigh, bounds.high });
		}
	}

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, bounds, ranges, markings, true);

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
	float threshold = msToTicks(*secondJumpThresholdMs);
	float success = threshold;
	float warning = 1.5f * threshold;
	float upperBound = 2.0f * threshold;
	Bounds bounds = { 0, upperBound };

	// Let the bar fill up when not dodged already.
	auto ticks = 0;
	if (attempt.dodged) ticks = attempt.dodgedTick - attempt.jumpTick;
	else if (attempt.jumped) ticks = attempt.currentTick - attempt.jumpTick;

	list<MeterRange> ranges;
	list<MeterMarking> markings;

	struct Color meterColor = ticks <= success
		? GREEN()
		: ticks <= warning
		? YELLOW()
		: RED();
	ranges.push_back({ meterColor, bounds.low, (float)ticks });

	markings.push_back({ WHITE(), BORDER.width, success });
	markings.push_back({ WHITE(), BORDER.width, warning });
	markings.push_back({ MARKER, (float)ticks });

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, bounds, ranges, markings, true);

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
	float threshold = msToTicks(*flipCancelThresholdMs);
	float success = threshold;
	float warning = 1.5f * threshold;
	float upperBound = 2.0f * threshold;
	Bounds bounds = { 0, upperBound };

	// Let the bar fill up when not cancelled already.
	auto ticks = 0;
	if (attempt.flipCanceled) ticks = attempt.flipCancelTick - attempt.dodgedTick;
	else if (attempt.dodged) ticks = attempt.currentTick - attempt.dodgedTick;

	list<MeterRange> ranges;
	list<MeterMarking> markings;

	Color meterColor = ticks <= success
		? GREEN()
		: ticks <= warning
		? YELLOW()
		: RED();
	ranges.push_back({ meterColor, bounds.low, (float)ticks });

	markings.push_back({ WHITE(), BORDER.width, success });
	markings.push_back({ WHITE(), BORDER.width, warning });
	markings.push_back({ MARKER, (float)ticks });

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, bounds, ranges, markings, true);

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
	float greenRange = 8, yellowRange = 15;
	float leftTarget = -(*targetAngle);
	float rightTarget = *targetAngle;
	Bounds bounds = { -90, 90 }; // Cap angle at 90 degrees

	list<MeterRange> ranges;
	list<MeterMarking> markings;

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

	float previewOpacity = 0.2;
	ranges.push_back({ YELLOW(previewOpacity), leftYellowLow, leftGreenLow });
	ranges.push_back({ GREEN(previewOpacity), leftGreenLow, leftGreenHigh });
	ranges.push_back({ YELLOW(previewOpacity), leftGreenHigh, leftYellowHigh });
	ranges.push_back({ YELLOW(previewOpacity), rightYellowLow, rightGreenLow });
	ranges.push_back({ GREEN(previewOpacity), rightGreenLow, rightGreenHigh });
	ranges.push_back({ YELLOW(previewOpacity), rightGreenHigh, rightYellowHigh });

	float angle = attempt.dodgeAngle;

	// Always render the marker, because we compute a "preview" angle before dodging.
	markings.push_back({ MARKER, angle });

	if (attempt.dodged)
	{
		if (angle < leftYellowLow)
		{
			ranges.push_back({ RED(), bounds.low, leftYellowLow });
		}
		else if (angle < leftGreenLow)
		{
			ranges.push_back({ YELLOW(), leftTarget - yellowRange, leftTarget - greenRange });
		}
		else if (angle <= leftGreenHigh)
		{
			ranges.push_back({ GREEN(), leftTarget - greenRange, leftTarget + greenRange });
		}
		else if (angle <= leftYellowHigh)
		{
			ranges.push_back({ YELLOW(), leftTarget + greenRange, leftTarget + yellowRange });
		}
		else if (angle < rightYellowLow)
		{
			ranges.push_back({ RED(), leftTarget + yellowRange, rightTarget - yellowRange });
		}
		else if (angle < rightGreenLow)
		{
			ranges.push_back({ YELLOW(), rightTarget - yellowRange, rightTarget - greenRange });
		}
		else if (angle <= rightGreenHigh)
		{
			ranges.push_back({ GREEN(), rightTarget - greenRange, rightTarget + greenRange });
		}
		else if (angle <= rightYellowHigh)
		{
			ranges.push_back({ YELLOW(), rightTarget + greenRange, rightTarget + yellowRange });
		}
		else
		{
			ranges.push_back({ RED(), rightTarget + yellowRange, bounds.high });
		}
	}

	RenderMeter(canvas, startPos, boxSize, BACKGROUND, BORDER, { -90, 90 }, ranges, markings, false);

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

#pragma once

struct Attempt
{
	// Whether the attempt started before the car touched the ground
	bool startedInAir = false;

	// Whether the attempt started without pressing boost
	bool startedNoBoost = false;

	// Variables to measure the first jump
	int jumpTick = 0;
	bool jumped = false;

	// Variables to measure the flip cancel
	int flipCancelTick = 0;
	bool flipCanceled = false;

	// Variables to measure the dodge angle
	int dodgeAngle = 0;
	int dodgedTick = 0;
	bool dodged = false;

	// Tracking sideways movement
	float traveledSideways = 0;
	Vector kickoffDirection = Vector(0, 0, 0);
	Vector startingLocation = Vector(0, 0, 0);
	Vector currentLocation = Vector(0, 0, 0);

	// Number of ticks taken to reach the ball
	int ticksToBall = 0;
	float timeToBall = 0.0f;

	// Was the ball already hit?
	bool hit() const { return ticksToBall > 0; }

	// Number of ticks not pressing boost or throttle
	int ticksNotPressingBoost = 0;
	int ticksNotPressingThrottle = 0;
};

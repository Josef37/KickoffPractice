#pragma once

#include <vector>
#include <string>
#include <ctime>
#include <filesystem>

#include "bakkesmod/wrappers/wrapperstructs.h"

enum KickoffPosition
{
	CornerRight = 0,
	CornerLeft = 1,
	BackRight = 2,
	BackLeft = 3,
	BackCenter = 4,
};
enum class KickoffSide
{
	Blue,
	Orange,
};
enum class KickoffState
{
	// Kickoff is over or countdown wasn't started.
	nothing,
	// Countdown is active. Cars are not moving.
	waitingToStart,
	// Countdown is over. Bot and player are moving.
	// Kickoff is considered over after ball hit + `timeAfterBackToNormal`. 
	started
};
enum class KickoffMode
{
	Training,
	Recording,
	Replaying
};

struct RecordedKickoff
{
	// Equals the file name (without extension).
	std::string name;
	// Is it selected for training?
	bool isActive = false;

	// Recording header/config
	KickoffPosition position = KickoffPosition::CornerLeft;
	int carBody = 23; // Octane
	GamepadSettings settings = GamepadSettings(0, 0.5, 1, 1);

	// Recorded inputs
	std::vector<ControllerInput> inputs;
};

namespace Utils
{
	using enum KickoffPosition;
	using enum KickoffSide;

	inline std::string getCurrentTimestamp()
	{
		auto time = std::time(nullptr);
		std::ostringstream oss;
		oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H-%M-%S");
		return oss.str();
	}

	// TODO: Use this whenever looping all positions.
	inline std::vector<KickoffPosition> getAllKickoffPositions()
	{
		return {
			CornerRight,
			CornerLeft,
			BackRight,
			BackLeft,
			BackCenter
		};
	}

	inline Vector getKickoffLocation(KickoffPosition kickoff, KickoffSide side)
	{
		const Vector heightOffset = Vector(0, 0, 20);

		if (side == Blue)
		{
			if (kickoff == CornerRight)
				return Vector(-2048, -2560, 0) + heightOffset;
			if (kickoff == CornerLeft)
				return Vector(2048, -2560, 0) + heightOffset;
			if (kickoff == BackRight)
				return Vector(-256, -3840, 0) + heightOffset;
			if (kickoff == BackLeft)
				return Vector(256.0, -3840, 0) + heightOffset;
			if (kickoff == BackCenter)
				return Vector(0.0, -4608, 0) + heightOffset;
		}
		else
		{
			return -1 * getKickoffLocation(kickoff, Blue) + (2 * heightOffset);
		}
	}

	inline KickoffPosition getKickoffForLocation(Vector location)
	{
		float closestDistance = std::numeric_limits<float>::infinity();
		KickoffPosition closestPosition = BackCenter;

		for (KickoffSide side : { Blue, Orange })
		{
			for (KickoffPosition position : getAllKickoffPositions())
			{
				auto kickoffLoation = getKickoffLocation(position, side);
				auto distance = (kickoffLoation - location).magnitude();

				if (distance < closestDistance)
				{
					closestDistance = distance;
					closestPosition = position;
				}
			}
		}

		return closestPosition;
	}

	inline float getKickoffYaw(KickoffPosition kickoff, KickoffSide side)
	{
		if (side == Blue)
		{
			if (kickoff == CornerRight)
				return 0.25f * CONST_PI_F;
			if (kickoff == CornerLeft)
				return 0.75f * CONST_PI_F;
			if (kickoff == BackRight)
				return 0.5f * CONST_PI_F;
			if (kickoff == BackLeft)
				return 0.5f * CONST_PI_F;
			if (kickoff == BackCenter)
				return 0.5f * CONST_PI_F;
		}
		else
		{
			return getKickoffYaw(kickoff, Blue) - CONST_PI_F;
		}
	}

	inline Rotator getKickoffRotation(KickoffPosition kickoff, KickoffSide side)
	{
		float yaw = getKickoffYaw(kickoff, side);
		return Rotator(0, std::lroundf(yaw * CONST_RadToUnrRot), 0);
	}

	inline std::string getKickoffPositionName(int kickoff)
	{
		switch (kickoff)
		{
		case CornerRight:
			return "Corner Right";
		case CornerLeft:
			return "Corner Left";
		case BackRight:
			return "Back Right";
		case BackLeft:
			return "Back Left";
		case BackCenter:
			return "Back Center";
		default:
			return "Unknown";
		}
	}
}

#pragma once

#include <vector>
#include <string>
#include <ctime>
#include <filesystem>

#include "bakkesmod/wrappers/wrapperstructs.h"

enum class KickoffPosition
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
	Nothing,
	// Countdown is active. Cars are not moving.
	WaitingToStart,
	// Countdown is over. Bot and player are moving.
	// Kickoff is considered over after ball hit + `timeAfterBackToNormal`. 
	Started
};
enum class KickoffMode
{
	Training,
	Recording,
	Replaying
};

enum class GameMode
{
	Soccar,
	Hoops,
	Dropshot,
	Snowday
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
	GameMode gameMode = GameMode::Soccar;

	// Recorded inputs
	std::vector<ControllerInput> inputs;
};

struct BoostSettings
{
	int UnlimitedBoostRefCount;
	unsigned long NoBoost;
	float RechargeDelay;
	float RechargeRate;
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

	inline const std::vector<KickoffPosition> allKickoffPositions = {
		CornerRight,
		CornerLeft,
		BackRight,
		BackLeft,
		BackCenter
	};

	// Avoid casting everywhere... Do it only here!
	inline int positionToInt(KickoffPosition position) { return static_cast<int>(position); }
	inline KickoffPosition positionFromInt(int position) { return static_cast<KickoffPosition>(position); }
	inline int gameModeToInt(GameMode gameMode) { return static_cast<int>(gameMode); }
	inline GameMode gameModeFromInt(int gameMode) { return static_cast<GameMode>(gameMode); }

	inline Vector getKickoffLocation(KickoffPosition kickoff, KickoffSide side, GameMode gameMode)
	{
		auto soccar = [&](KickoffPosition kickoff)
			{
				if (kickoff == CornerRight)	return Vector(-2048, -2560, 0);
				if (kickoff == CornerLeft)	return Vector(2048, -2560, 0);
				if (kickoff == BackRight)	return Vector(-256, -3840, 0);
				if (kickoff == BackLeft)	return Vector(256, -3840, 0);
				if (kickoff == BackCenter)	return Vector(0, -4608, 0);
			};
		auto hoops = [&](KickoffPosition kickoff)
			{
				if (kickoff == CornerRight)	return Vector(-1536, -3072, 0);
				if (kickoff == CornerLeft)	return Vector(1536, -3072, 0);
				if (kickoff == BackRight)	return Vector(-256, -2816, 0);
				if (kickoff == BackLeft)	return Vector(256, -2816, 0);
				if (kickoff == BackCenter)	return Vector(0, -3200, 0);
			};
		auto dropshot = [&](KickoffPosition kickoff)
			{
				if (kickoff == CornerRight)	return Vector(-1867, -2379, 0);
				if (kickoff == CornerLeft)	return Vector(1867, -2379, 0);
				if (kickoff == BackRight)	return Vector(-256, -3576, 0);
				if (kickoff == BackLeft)	return Vector(256, -3576, 0);
				if (kickoff == BackCenter)	return Vector(0, -4088, 0);
			};

		auto getGroundLocation = [&](KickoffPosition kickoff, GameMode gameMode)
			{
				if (gameMode == GameMode::Soccar)	return soccar(kickoff);
				if (gameMode == GameMode::Snowday)	return soccar(kickoff);
				if (gameMode == GameMode::Hoops)	return hoops(kickoff);
				if (gameMode == GameMode::Dropshot) return dropshot(kickoff);
			};


		auto groundLocation = getGroundLocation(kickoff, gameMode);
		if (side == Orange) groundLocation *= -1;

		auto heightOffset = Vector(0, 0, 20);

		return groundLocation + heightOffset;
	}

	inline float getKickoffYaw(KickoffPosition kickoff, KickoffSide side, GameMode gameMode)
	{
		if (side == Blue)
		{
			if (gameMode == GameMode::Hoops) return 0.5f * CONST_PI_F;

			if (kickoff == CornerRight) return 0.25f * CONST_PI_F;
			if (kickoff == CornerLeft)	return 0.75f * CONST_PI_F;
			if (kickoff == BackRight)	return 0.5f * CONST_PI_F;
			if (kickoff == BackLeft)	return 0.5f * CONST_PI_F;
			if (kickoff == BackCenter)	return 0.5f * CONST_PI_F;
		}
		else
		{
			return getKickoffYaw(kickoff, Blue, gameMode) - CONST_PI_F;
		}
	}

	inline Rotator getKickoffRotation(KickoffPosition kickoff, KickoffSide side, GameMode gameMode)
	{
		float yaw = getKickoffYaw(kickoff, side, gameMode);
		return Rotator(0, std::lroundf(yaw * CONST_RadToUnrRot), 0);
	}

	inline Vector getKickoffBallLocation(GameMode gameMode)
	{
		if (gameMode == GameMode::Soccar)	return Vector(0, 0, 92.75);
		if (gameMode == GameMode::Hoops)	return Vector(0, 0, 98.37);
		if (gameMode == GameMode::Dropshot) return Vector(0, 0, 101.58);
		if (gameMode == GameMode::Snowday)	return Vector(0, 0, 32.57);
		return Vector();
	}

	inline Vector getKickoffBallVelocity(GameMode gameMode)
	{
		if (gameMode == GameMode::Soccar)	return Vector(0, 0, 0);
		if (gameMode == GameMode::Hoops)	return Vector(0, 0, 999.99);
		if (gameMode == GameMode::Dropshot) return Vector(0, 0, 999.99);
		if (gameMode == GameMode::Snowday)	return Vector(0, 0, 0);
		return Vector();
	}

	inline std::string getKickoffPositionName(KickoffPosition kickoff)
	{
		if (kickoff == CornerRight) return "Corner Right";
		if (kickoff == CornerLeft)	return "Corner Left";
		if (kickoff == BackRight)	return "Back Right";
		if (kickoff == BackLeft)	return "Back Left";
		if (kickoff == BackCenter)	return "Back Center";
		return "Unknown";
	}

	inline std::string getGameModeName(GameMode gameMode)
	{
		if (gameMode == GameMode::Soccar)	return "Soccar";
		if (gameMode == GameMode::Hoops)	return "Hoops";
		if (gameMode == GameMode::Dropshot)	return "Dropshot";
		if (gameMode == GameMode::Snowday)	return "Snowday";
		return "Unknown";
	}

	// Determine the current gamemode by the ball radius.
	// There might be a better way, but I can't figure it out.
	// Also: When calling `load_freeplay` for non-soccar maps, it won't load the right ball.
	inline std::optional<GameMode> determineGameMode(BallWrapper ball)
	{
		auto radius = ball.GetRadius();

		auto radiusIsCloseTo = [&](float expected) { return abs(radius - expected) < 0.1f; };

		if (radiusIsCloseTo(95.49))  return GameMode::Soccar;
		if (radiusIsCloseTo(98.13))  return GameMode::Hoops;
		if (radiusIsCloseTo(29.0))	 return GameMode::Snowday;
		if (radiusIsCloseTo(102.01)) return GameMode::Dropshot;

		LOG("Can't determine gamemode from ball radius: {}", radius);
		return std::nullopt;
	}

	inline float getInitialBoostAmount(GameMode gameMode)
	{
		if (gameMode == GameMode::Dropshot)
			return 1.0f;

		return 0.333f;
	}

	inline BoostSettings getInitialBoostSettings(GameMode gameMode)
	{
		BoostSettings settings = {
			.UnlimitedBoostRefCount = 0,
			.NoBoost = false,
			.RechargeDelay = 0.f,
			.RechargeRate = 0.f
		};

		if (gameMode == GameMode::Dropshot)
		{
			settings.RechargeDelay = 0.25f;
			settings.RechargeRate = 0.1f;
		}

		return settings;
	}

	template<typename T>
	inline void removeFromVector(std::vector<T>& vector, T& kickoff)
	{
		auto it = std::find_if(
			vector.begin(),
			vector.end(),
			[&](T& other) { return other == kickoff; }
		);

		if (it == vector.end())
			return;

		vector.erase(it);
	}
}

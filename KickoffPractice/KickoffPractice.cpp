#include "pch.h"
#include "KickoffPractice.h"

BAKKESMOD_PLUGIN(KickoffPractice, "Kickoff Practice", plugin_version, PLUGINTYPE_FREEPLAY);

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
namespace fs = std::filesystem;

static const float INITIAL_BOOST_AMOUNT = 0.333f;
static const BoostSettings INITIAL_BOOST_SETTINGS = BoostSettings{
	.UnlimitedBoostRefCount = 0,
	.NoBoost = false,
	.RechargeDelay = 0.f,
	.RechargeRate = 0.f
};

static const std::string PLUGIN_FOLDER = "kickoffPractice";
static const std::string CONFIG_FILE = "config.cfg";
static const std::string DEFAULT_BOT_FOLDER = "bot";
static const std::string DEFAULT_RECORDING_FOLDER = "recorded";
static const std::string FILE_EXT = ".kinputs";
static const std::string BOT_CAR_NAME = "Kickoff Bot";

void KickoffPractice::onLoad()
{
	_globalCvarManager = cvarManager;

	this->pluginEnabled = true;
	this->isInGoalReplay = false;
	this->currentKickoff = nullptr;
	this->mode = KickoffMode::Training;
	this->rotationBot = Rotator(0, 0, 0);
	this->locationBot = Vector(0, 0, 0);
	this->startingFrame = 0;
	this->kickoffCounter = 0;
	this->kickoffState = KickoffState::nothing;
	this->botJustSpawned = false;
	this->currentKickoffPosition = KickoffPosition::CornerRight;
	srand((int)time(0)); // initialize the random number generator seed

	persistentStorage = std::make_shared<PersistentStorage>(this, "kickoffPractice", true, true);

	// TODO: Add description. Store cvar, title and description in variable to use it in UI, too.
	persistentStorage->RegisterPersistentCvar(CVAR_ENABLED, "1").addOnValueChanged([this](std::string oldValue, CVarWrapper cvar)
		{
			pluginEnabled = cvar.getBoolValue();
			if (!pluginEnabled) gameWrapper->Execute([this](...) { reset(); });
		});

	persistentStorage->RegisterPersistentCvar(CVAR_RESTART_ON_RESET, "1")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { restartOnTrainingReset = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_AUTO_RESTART, "0")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { autoRestart = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_INDICATOR, "1")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { showIndicator = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_BACK_TO_NORMAL, "0.5")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { timeAfterBackToNormal = cvar.getFloatValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_ACTIVE_POSITIONS, getActivePositionsMask())
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { setActivePositionFromMask(cvar.getStringValue()); });

	this->configPath = gameWrapper->GetDataFolder() / PLUGIN_FOLDER;
	if (!fs::exists(this->configPath) || !fs::is_directory(this->configPath))
		if (!fs::create_directory(this->configPath))
			LOG("Can't create config directory in bakkesmod data folder");

	this->readKickoffFiles();

	cvarManager->registerNotifier(TRAIN_COMMAND,
		[this](std::vector<std::string> args)
		{
			if (!this->shouldExecute()) return;

			this->mode = KickoffMode::Training;

			this->positionOverride = args.size() >= 2
				? parseKickoffArg(args[1])
				: std::nullopt;

			this->start();
		},
		"Practice kickoff. Without arguments: Selected positions. With argument from 1 to 5: Specific kickoff position.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(RECORD_COMMAND,
		[this](std::vector<std::string> args)
		{
			if (!this->shouldExecute()) return;

			this->mode = KickoffMode::Recording;

			std::optional<KickoffPosition> position = args.size() >= 2
				? parseKickoffArg(args[1])
				: std::nullopt;

			if (position == std::nullopt)
			{
				LOG("Kickoff number argument expected for recordings.");
				return;
			}

			this->currentKickoffPosition = *position;

			this->start();
		},
		"Record a kickoff. Specify kickoff position with index from 1 to 5.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(REPLAY_COMMAND,
		[this](std::vector<std::string> args)
		{
			if (!this->shouldExecute()) return;

			this->mode = KickoffMode::Replaying;

			if (args.size() < 2)
			{
				LOG("Kickoff name argument expected for replaying.");
				return;
			}
			auto& kickoffName = args[1];
			auto findByName = [&](const RecordedKickoff& kickoff) { return kickoff.name == kickoffName; };
			auto it = std::find_if(this->loadedKickoffs.begin(), this->loadedKickoffs.end(), findByName);
			if (it == this->loadedKickoffs.end())
			{
				LOG("No kickoff found with the specified name: {}", kickoffName);
				return;
			}
			auto& foundKickoff = *it;
			this->currentKickoff = &foundKickoff;
			this->currentKickoffPosition = this->currentKickoff->position;

			this->start();
		},
		"Replay a kickoff. Spawns a bot that replays the same recording.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(SAVE_COMMAND,
		[this](std::vector<std::string> args)
		{
			// Don't check `shouldExecute()`. We always allow saving.
			this->saveRecording();
		},
		"Save the last kickoff. Recordings are saved automatically.",
		PERMISSION_FREEPLAY
	);

	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.Car_TA.SetVehicleInput",
		[this](CarWrapper car, void* params, std::string eventName)
		{
			if (!this->shouldExecute()) return;

			ControllerInput* input = static_cast<ControllerInput*>(params);
			this->onVehicleInput(car, input);
		}
	);

	gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function TAGame.Car_TA.OnHitBall",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			if (!this->shouldExecute()) return;
			if (this->kickoffState != KickoffState::started) return;

			this->setTimeoutChecked(
				this->timeAfterBackToNormal,
				[this]()
				{
					if (this->kickoffState != KickoffState::started) return;

					if (this->mode == KickoffMode::Recording)
						this->saveRecording();

					this->reset();

					if (this->autoRestart)
						this->start();
				}
			);
		}
	);

	gameWrapper->HookEvent(
		"Function Engine.Actor.SpawnInstance",
		[this](...)
		{
			if (!this->shouldExecute()) return;

			ServerWrapper server = gameWrapper->GetCurrentGameState();
			if (!server) return;
			// `SpawnInstance` gets called multiple times for the same car.
			// We make sure to execute the following code only once.
			if (!this->botJustSpawned) return;
			if (!this->currentKickoff) return;

			for (auto car : server.GetCars())
			{
				if (!this->isBot(car)) continue;

				car.SetLocation(this->locationBot);
				car.SetCarRotation(this->rotationBot);
				car.Stop();
				// To disable the bot moving by itself we would call `car.GetAIController().DoNothing()` here.
				// But then the `SetVehicleInput` hook would not fire for the bot.
				// So we don't disable the controller, but overwrite the inputs inside the hook.

				auto settings = this->currentKickoff->settings;
				car.GetPRI().SetUserCarPreferences(settings.DodgeInputThreshold, settings.SteeringSensitivity, settings.AirControlSensitivity);
			}
			this->botJustSpawned = false;
		}
	);

	gameWrapper->HookEventPost(
		"Function TAGame.Ball_TA.OnHitGoal",
		[this](...)
		{
			this->isInGoalReplay = true;
		}
	);
	gameWrapper->HookEventPost(
		"Function GameEvent_Soccar_TA.ReplayPlayback.EndState",
		[this](...)
		{
			this->isInGoalReplay = false;
		}
	);
	gameWrapper->HookEventPost(
		"Function GameEvent_Soccar_TA.Countdown.EndState", // Called at the beginning/reset of freeplay.
		[this](...)
		{
			if (!this->shouldExecute()) return;

			this->recordBoostSettings();
			this->reset();
		}
	);
	gameWrapper->HookEvent(
		"Function TAGame.PlayerController_TA.PlayerResetTraining", // Called when resetting freeplay.
		[this](...)
		{
			// Allow to break out of auto-restart by resetting freeplay.
			if (this->autoRestart && this->kickoffState != KickoffState::nothing)
				return;
			if (this->restartOnTrainingReset)
				this->start();
		}
	);

	gameWrapper->RegisterDrawable(
		[this](CanvasWrapper canvas)
		{
			this->renderIndicator(canvas);
		}
	);
}

void KickoffPractice::onUnload()
{
	this->reset();
}

bool KickoffPractice::shouldExecute()
{
	return pluginEnabled && gameWrapper->IsInFreeplay() && !this->isInGoalReplay;
}

void KickoffPractice::setTimeoutChecked(float seconds, std::function<void()> callback)
{
	gameWrapper->SetTimeout(
		[this, callback](...)
		{
			if (!this->shouldExecute())
				return;

			callback();
		},
		seconds
	);
}

void KickoffPractice::start()
{
	this->kickoffCounter++;

	this->recordBoostSettings();
	this->reset();

	// Determine a suitable kickoff for training here, so we can call `start()` when
	// resetting freeplay to repeat the last command (and not repeat the same kickoff).
	if (this->mode == KickoffMode::Training)
	{
		std::vector<RecordedKickoff*> suitableKickoffs;
		for (auto& kickoff : this->loadedKickoffs)
		{
			bool isSuitable = kickoff.isActive;

			if (this->positionOverride.has_value())
				isSuitable = isSuitable && kickoff.position == *this->positionOverride;
			else
				isSuitable = isSuitable && this->activePositions.contains(kickoff.position);

			if (isSuitable)
				suitableKickoffs.push_back(&kickoff);
		}

		if (suitableKickoffs.empty())
		{
			LOG("No suitable kickoff recording found! Make sure there are active kickoffs for your selected positions.");
			return;
		}

		this->currentKickoff = suitableKickoffs[rand() % suitableKickoffs.size()];
		this->currentKickoffPosition = this->currentKickoff->position;
	}

	// We wait a little before updating the game state, because we want the reset to finish first.
	// Otherwise `player.SetLocation()` won't work properly, because `player.SetbDriving(false)` still affects it.
	this->setTimeoutChecked(
		gameWrapper->GetEngine().GetBulletFixedDeltaTime(),
		[this]() { this->setupKickoff(); }
	);
}

void KickoffPractice::setupKickoff()
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	CarWrapper player = gameWrapper->GetLocalCar();
	if (!player) return;
	BoostWrapper boost = player.GetBoostComponent();
	if (!boost) return;
	BallWrapper ball = server.GetBall();
	if (!ball) return;

	KickoffSide playerSide = this->mode == KickoffMode::Recording ? KickoffSide::Orange : KickoffSide::Blue;
	Vector locationPlayer = KickoffPractice::getKickoffLocation(this->currentKickoffPosition, playerSide);
	Rotator rotationPlayer = Rotator(0, std::lroundf(KickoffPractice::getKickoffYaw(this->currentKickoffPosition, playerSide) * CONST_RadToUnrRot), 0);
	this->locationBot = KickoffPractice::getKickoffLocation(this->currentKickoffPosition, KickoffSide::Orange);
	this->rotationBot = Rotator(0, std::lroundf(KickoffPractice::getKickoffYaw(this->currentKickoffPosition, KickoffSide::Orange) * CONST_RadToUnrRot), 0);
	if (this->mode != KickoffMode::Recording)
	{
		auto carBody = this->currentKickoff->carBody;
		server.SpawnBot(carBody, BOT_CAR_NAME);
		this->botJustSpawned = true;
	}

	player.SetLocation(locationPlayer);
	player.SetRotation(rotationPlayer);
	player.Stop();

	KickoffPractice::applyBoostSettings(boost, INITIAL_BOOST_SETTINGS);
	boost.SetCurrentBoostAmount(INITIAL_BOOST_AMOUNT);

	// Reset boost pickups, because moving the player can cause picking up boost.
	this->setTimeoutChecked(
		gameWrapper->GetEngine().GetBulletFixedDeltaTime(),
		[this]()
		{
			if (auto server = gameWrapper->GetCurrentGameState())
				server.ResetPickups();
		}
	);

	ball.SetLocation(Vector(0, 0, ball.GetRadius()));
	ball.SetVelocity(Vector(0, 0, 0));
	ball.SetAngularVelocity(Vector(0, 0, 0), false);
	ball.SetRotation(Rotator(0, 0, 0));

	this->kickoffState = KickoffState::waitingToStart;

	// TODO: Align the countdown end with the physics frames for more consistency.
	startCountdown(
		this->mode == KickoffMode::Replaying ? 1 : 3,
		this->kickoffCounter,
		[this]()
		{
			this->recordedInputs.clear();
			this->kickoffState = KickoffState::started;
			this->startingFrame = gameWrapper->GetEngine().GetPhysicsFrame();
		}
	);
}

void KickoffPractice::startCountdown(int seconds, int kickoffCounterAtStart, std::function<void()> onCompleted)
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	if (this->kickoffState != KickoffState::waitingToStart) return;

	// Abort the countdown, if we restarted or aborted the kickoff.
	if (this->kickoffCounter != kickoffCounterAtStart) return;

	if (seconds <= 0)
	{
		server.SendGoMessage(gameWrapper->GetPlayerController());
		onCompleted();

		return;
	}

	// TODO: Actually pause the countdown.
	if (!gameWrapper->IsPaused())
		server.SendCountdownMessage(seconds, gameWrapper->GetPlayerController());

	// TODO: Verify the countdown is not delayed too much because the timeout might only be a lower bound.
	this->setTimeoutChecked(
		1.0f,
		[this, seconds, kickoffCounterAtStart, onCompleted]()
		{
			auto newDelay = gameWrapper->IsPaused() ? seconds : seconds - 1;
			this->startCountdown(newDelay, kickoffCounterAtStart, onCompleted);
		}
	);
}

static const ControllerInput EMPTY_INPUT = ControllerInput{
	.Throttle = 0,
	.Steer = 0,
	.Pitch = 0,
	.Yaw = 0,
	.Roll = 0,
	.DodgeForward = 0,
	.DodgeStrafe = 0,
	.Handbrake = 0,
	.Jump = 0,
	.ActivateBoost = 0,
	.HoldingBoost = 0,
	.Jumped = 0,
};

void KickoffPractice::onVehicleInput(CarWrapper car, ControllerInput* input)
{
	if (this->isBot(car))
	{
		auto& bot = car;

		*input = EMPTY_INPUT;

		if (this->kickoffState != KickoffState::started)
			return;

		// The Bot Controller calls this functions multiple times per tick (varies by game speed).
		// We need to look at the elapsed time/ticks to get the correct input.
		auto recordedInput = getRecordedInput();
		if (recordedInput.has_value())
			*input = *recordedInput;
		else
			this->removeBot(bot);
	}
	else
	{
		auto& player = car;

		player.SetbDriving(this->kickoffState != KickoffState::waitingToStart);

		// If the player is holding boost when starting training, it won't stop consuming boost.
		// Disabling was quite messy (with `PlayerController::ToggleBoost` or `BoostComponent::SetbActive`),
		// which resulted in weird game states. So we just fill the tank every tick.
		if (auto boost = player.GetBoostComponent())
			if (this->kickoffState == KickoffState::waitingToStart)
				boost.SetCurrentBoostAmount(INITIAL_BOOST_AMOUNT);

		if (this->kickoffState != KickoffState::started)
			return;

		if (this->mode == KickoffMode::Replaying)
		{
			auto recordedInput = getRecordedInput();
			if (recordedInput.has_value())
				*input = *recordedInput;
		}

		this->recordedInputs.push_back(*input);
	}
}

std::optional<ControllerInput> KickoffPractice::getRecordedInput()
{
	if (this->currentKickoff == nullptr)
		return std::nullopt;

	auto& inputs = this->currentKickoff->inputs;
	auto currentFrame = gameWrapper->GetEngine().GetPhysicsFrame();
	auto tick = currentFrame - this->startingFrame;

	if (tick >= inputs.size())
		return std::nullopt;

	return inputs[tick];
}

void KickoffPractice::reset()
{
	this->removeBots();
	this->kickoffState = KickoffState::nothing;
	this->resetBoostSettings();
	this->isInGoalReplay = false;
}

void KickoffPractice::saveRecording()
{
	if (this->recordedInputs.empty())
	{
		LOG("Nothing recorded.");
		return;
	}
	LOG("Saving... Ticks recorded: {}", this->recordedInputs.size());

	RecordedKickoff kickoff;
	kickoff.name = getNewRecordingName();
	kickoff.position = this->currentKickoffPosition;
	kickoff.carBody = gameWrapper->GetLocalCar().GetLoadoutBody(); // TODO: Improve by using the values during the recording.
	kickoff.settings = gameWrapper->GetSettings().GetGamepadSettings();
	kickoff.inputs = this->recordedInputs;
	kickoff.isActive = true; // Automatically select the newly recorded kickoff.

	this->loadedKickoffs.push_back(kickoff);
	this->writeActiveKickoffs();

	auto filename = kickoff.name + FILE_EXT;
	std::ofstream inputFile(this->configPath / filename);
	if (!inputFile.is_open())
	{
		LOG("ERROR : can't create recording file");
		return;
	}

	inputFile << "position:" << kickoff.position << "\n";
	inputFile << "carBody:" << kickoff.carBody << "\n";

	inputFile << "settings:" << kickoff.settings.ControllerDeadzone
		<< "," << kickoff.settings.DodgeInputThreshold
		<< "," << kickoff.settings.SteeringSensitivity
		<< "," << kickoff.settings.AirControlSensitivity
		<< "\n";

	inputFile << "inputs" << "\n";
	for (const ControllerInput& input : kickoff.inputs)
	{
		inputFile << input.Throttle
			<< "," << input.Steer
			<< "," << input.Pitch
			<< "," << input.Yaw
			<< "," << input.Roll
			<< "," << input.DodgeForward
			<< "," << input.DodgeStrafe
			<< "," << input.Handbrake
			<< "," << input.Jump
			<< "," << input.ActivateBoost
			<< "," << input.HoldingBoost
			<< "," << input.Jumped
			<< "\n";
	}
	inputFile.close();
}

std::string KickoffPractice::getNewRecordingName() const
{
	auto time = std::time(nullptr);
	std::ostringstream oss;
	oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H-%M-%S");
	std::string timestamp = oss.str();

	std::string kickoffName = KickoffPractice::getKickoffName(this->currentKickoffPosition);

	return timestamp + " " + kickoffName;
}

void KickoffPractice::removeBots()
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	for (auto car : server.GetCars())
	{
		if (this->isBot(car))
			this->removeBot(car);
	}
}

void KickoffPractice::removeBot(CarWrapper car)
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	// To avoid the lightning that shows on `server.RemovePlayer()` we call `car.Destory()` first.
	// After `car.Destory()` `car.GetAIController()` wouldn't work, so we have to store it beforehand.
	// If we don't call `server.RemovePlayer()`, the bots will respawn on "Reset Ball".
	auto controller = car.GetAIController();
	car.Destroy();
	server.RemovePlayer(controller);
}

bool KickoffPractice::isBot(CarWrapper car)
{
	return car.GetOwnerName() == BOT_CAR_NAME && car.GetPRI().GetbBot();
}

void KickoffPractice::writeActiveKickoffs()
{
	auto filename = this->configPath / CONFIG_FILE;

	std::ofstream inputFile(filename);

	if (!inputFile.is_open())
	{
		LOG("Can't create the config file");
		return;
	}

	for (auto& kickoff : this->loadedKickoffs)
	{
		if (!kickoff.isActive) continue;
		inputFile << kickoff.name << "\n";
	}
	inputFile.close();
}

void KickoffPractice::readActiveKickoffs()
{
	auto filename = this->configPath / CONFIG_FILE;

	if (!fs::exists(filename))
		return;

	std::fstream file(filename, std::ios::in);

	if (!file.is_open())
	{
		LOG("Can't open the config file");
		return;
	}

	std::string line;

	while (getline(file, line))
	{
		// TODO: Avoid iterating every line.
		for (auto& kickoff : this->loadedKickoffs)
		{
			if (kickoff.name == line)
				kickoff.isActive = true;
		}
	}
	file.close();
}

void KickoffPractice::readKickoffFiles()
{
	this->loadedKickoffs.clear();

	try
	{
		for (const auto& entry : fs::directory_iterator(this->configPath))
		{
			if (entry.is_regular_file() && entry.path().extension() == FILE_EXT)
			{
				this->loadedKickoffs.push_back(this->readKickoffFile(entry.path()));
			}
		}
	}
	catch (std::filesystem::filesystem_error const& ex)
	{
		LOG("ERROR : {}", ex.code().message());
	}

	this->readActiveKickoffs();
}

RecordedKickoff KickoffPractice::readKickoffFile(std::filesystem::path filePath)
{
	std::optional<KickoffPosition> position;
	std::optional<int> carBody;
	std::optional<GamepadSettings> settings;
	std::vector<ControllerInput> inputs;

	std::vector<std::string> row;
	std::string line, word;

	std::fstream file(filePath, std::ios::in);
	if (file.is_open())
	{
		int i = 0;
		bool inHeader = true;
		while (getline(file, line))
		{
			i++;
			row.clear();

			std::stringstream str(line);

			if (inHeader)
			{
				std::string header;
				getline(str, header, ':');

				while (getline(str, word, ','))
					row.push_back(word);

				if (header == "inputs")
				{
					inHeader = false;
					continue;
				}
				else if (header == "carBody")
				{
					if (row.size() == 1)
						carBody = std::stoi(row[0]);
					else
						LOG("Error on line {}: size of {} instead of 1", i, row.size());
				}
				else if (header == "position")
				{
					if (row.size() == 1)
						position = static_cast<KickoffPosition>(std::stoi(row[0]));
					else
						LOG("Error on line {}: size of {} instead of 1", i, row.size());
				}
				else if (header == "settings")
				{
					if (row.size() == 4)
					{
						settings = GamepadSettings{};
						settings->ControllerDeadzone = std::stof(row[0]);
						settings->DodgeInputThreshold = std::stof(row[1]);
						settings->SteeringSensitivity = std::stof(row[2]);
						settings->AirControlSensitivity = std::stof(row[3]);
					}
					else
						LOG("Error on line {}: size of {} instead of 4", i, row.size());
				}
				else
				{
					// Unknown header... Don't log it, because it could spam the console.
				}

				continue;
			}

			while (getline(str, word, ','))
				row.push_back(word);

			if (row.size() != 12)
			{
				LOG("Error on line {} : size of {} instead of 12", i, row.size());
				continue;
			}

			ControllerInput input;
			input.Throttle = std::stof(row[0]);
			input.Steer = std::stof(row[1]);
			input.Pitch = std::stof(row[2]);
			input.Yaw = std::stof(row[3]);
			input.Roll = std::stof(row[4]);
			input.DodgeForward = std::stof(row[5]);
			input.DodgeStrafe = std::stof(row[6]);
			input.Handbrake = std::stoul(row[7]);
			input.Jump = std::stoul(row[8]);
			input.ActivateBoost = std::stoul(row[9]);
			input.HoldingBoost = std::stoul(row[10]);
			input.Jumped = std::stoul(row[11]);

			inputs.push_back(input);
		}
	}
	else
	{
		LOG("Can't open {}", filePath.string());
	}

	LOG("{}: {} inputs loaded", filePath.filename().string(), inputs.size());

	RecordedKickoff kickoff;
	kickoff.name = filePath.stem().string();

	if (position.has_value())
		kickoff.position = *position;
	else
		LOG("Header `position` not found.");

	if (carBody.has_value())
		kickoff.carBody = *carBody;
	else
		LOG("Header `carBody` not found.");

	if (settings.has_value())
		kickoff.settings = *settings;
	else
		LOG("Header `settings` not found.");

	kickoff.inputs = inputs;
	if (inputs.empty())
		LOG("No inputs found.");

	return kickoff;
}

void KickoffPractice::recordBoostSettings()
{
	// Starting a new kickoff without finishing the previous one
	// would result in us storing our overwritten boost setting.
	if (this->kickoffState != KickoffState::nothing) return;

	auto player = gameWrapper->GetLocalCar();
	if (!player) return;
	auto boost = player.GetBoostComponent();
	if (!boost) return;

	BoostSettings settings{};
	settings.UnlimitedBoostRefCount = boost.GetUnlimitedBoostRefCount();
	settings.NoBoost = boost.GetbNoBoost();
	settings.RechargeDelay = boost.GetRechargeDelay();
	settings.RechargeRate = boost.GetRechargeRate();
	this->boostSettings = settings;
}

void KickoffPractice::resetBoostSettings()
{
	CarWrapper player = gameWrapper->GetLocalCar();
	if (!player) return;
	BoostWrapper boost = player.GetBoostComponent();
	if (!boost) return;

	KickoffPractice::applyBoostSettings(boost, this->boostSettings);
}

void KickoffPractice::applyBoostSettings(BoostWrapper boost, BoostSettings settings)
{
	boost.SetUnlimitedBoostRefCount(settings.UnlimitedBoostRefCount);
	if (settings.UnlimitedBoostRefCount > 0)
		boost.SetCurrentBoostAmount(1.0f);
	boost.SetbNoBoost(settings.NoBoost);
	boost.SetRechargeDelay(settings.RechargeDelay);
	boost.SetRechargeRate(settings.RechargeRate);
}

void KickoffPractice::renderIndicator(CanvasWrapper canvas)
{
	if (!showIndicator) return;
	if (!shouldExecute()) return;
	if (kickoffState == KickoffState::nothing) return;

	canvas.SetColor(LinearColor(255, 255, 255, 255));

	auto text = mode == KickoffMode::Training ? "Training"
		: mode == KickoffMode::Recording ? "Recording"
		: mode == KickoffMode::Replaying ? "Replaying"
		: "";
	auto scale = 3.0f;
	auto offset = 0.05f;

	canvas.SetPosition((Vector2F{ offset, offset }) * canvas.GetSize());
	canvas.DrawString(text, scale, scale);
}

Vector KickoffPractice::getKickoffLocation(int kickoff, KickoffSide side)
{
	const Vector heightOffset = Vector(0, 0, 20);

	if (side == KickoffSide::Blue)
	{
		if (kickoff == KickoffPosition::CornerRight)
			return Vector(-2048, -2560, 0) + heightOffset;
		if (kickoff == KickoffPosition::CornerLeft)
			return Vector(2048, -2560, 0) + heightOffset;
		if (kickoff == KickoffPosition::BackRight)
			return Vector(-256, -3840, 0) + heightOffset;
		if (kickoff == KickoffPosition::BackLeft)
			return Vector(256.0, -3840, 0) + heightOffset;
		if (kickoff == KickoffPosition::BackCenter)
			return Vector(0.0, -4608, 0) + heightOffset;
	}
	else
	{
		return -1 * KickoffPractice::getKickoffLocation(kickoff, KickoffSide::Blue) + (2 * heightOffset);
	}
}

float KickoffPractice::getKickoffYaw(int kickoff, KickoffSide side)
{
	if (side == KickoffSide::Blue)
	{
		if (kickoff == KickoffPosition::CornerRight)
			return 0.25f * CONST_PI_F;
		if (kickoff == KickoffPosition::CornerLeft)
			return 0.75f * CONST_PI_F;
		if (kickoff == KickoffPosition::BackRight)
			return 0.5f * CONST_PI_F;
		if (kickoff == KickoffPosition::BackLeft)
			return 0.5f * CONST_PI_F;
		if (kickoff == KickoffPosition::BackCenter)
			return 0.5f * CONST_PI_F;
	}
	else
	{
		return KickoffPractice::getKickoffYaw(kickoff, KickoffSide::Blue) - CONST_PI_F;
	}
}

std::string KickoffPractice::getKickoffName(int kickoff)
{
	switch (kickoff)
	{
	case 0:
		return "Right Corner";
	case 1:
		return "Left Corner";
	case 2:
		return "Back Right";
	case 3:
		return "Back Left";
	case 4:
		return "Back Center";
	default:
		return "Unknown";
	}
}

std::optional<KickoffPosition> KickoffPractice::parseKickoffArg(std::string arg)
{
	const int kickoffNumber = stoi(arg);
	if (kickoffNumber < 1 || kickoffNumber > 5)
	{
		LOG("The kickoff number argument should be between 1 and 5 (included).");
		return std::nullopt;
	}
	return static_cast<KickoffPosition>(kickoffNumber - 1);
}

// Use a string mask for usability in commands.
std::string KickoffPractice::getActivePositionsMask()
{
	std::string mask = "00000";
	for (auto position : activePositions)
		mask[position] = '1';
	return mask;
}
void KickoffPractice::setActivePositionFromMask(std::string mask)
{
	activePositions.clear();

	for (int i = 0; i < 5; i++)
		if (mask[i] == '1')
			activePositions.insert(static_cast<KickoffPosition>(i));
}

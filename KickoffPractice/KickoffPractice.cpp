#include "pch.h"
#include "KickoffPractice.h"

BAKKESMOD_PLUGIN(KickoffPractice, "Kickoff Practice", plugin_version, PLUGINTYPE_FREEPLAY);

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

static const float INITIAL_BOOST_AMOUNT = 0.333f;
static const BoostSettings INITIAL_BOOST_SETTINGS = BoostSettings{
	.UnlimitedBoostRefCount = 0,
	.NoBoost = false,
	.RechargeDelay = 0.f,
	.RechargeRate = 0.f
};

static const std::string PLUGIN_FOLDER = "kickoffPractice";
static const std::string BOT_CAR_NAME = "Kickoff Bot";

void KickoffPractice::onLoad()
{
	_globalCvarManager = cvarManager;

	// initialize the random number generator seed
	srand((int)time(0));

	speedFlipTrainer = std::make_unique<SpeedFlipTrainer>(
		gameWrapper,
		cvarManager,
		[this]() { return showSpeedFlipTrainer && shouldExecute(); }
	);

	kickoffStorage = std::make_unique<KickoffStorage>(
		gameWrapper->GetDataFolder() / PLUGIN_FOLDER
	);
	readKickoffsFromFile();

	registerCvars();

	registerCommands();

	hookEvents();

	registerDrawables();

	// Initially set `isInGoalReplay` if we load the plugin during goal replay.
	if (auto server = gameWrapper->GetCurrentGameState())
		if (auto director = server.GetReplayDirector())
			this->isInGoalReplay = director.GetReplayTimeSeconds() > 0;
}

void KickoffPractice::registerCvars()
{
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

	persistentStorage->RegisterPersistentCvar(CVAR_SPEEDFLIP_TRAINER, "1")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { showSpeedFlipTrainer = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_BACK_TO_NORMAL, "0.5")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { timeAfterBackToNormal = cvar.getFloatValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_ACTIVE_POSITIONS, getActivePositionsMask())
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { setActivePositionFromMask(cvar.getStringValue()); });
}

void KickoffPractice::registerCommands()
{
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
			if (!kickoffIndexByName.contains(kickoffName))
			{
				LOG("No kickoff found with the specified name: {}", kickoffName);
				return;
			}
			this->setCurrentKickoffIndex(kickoffIndexByName[kickoffName]);

			this->start();
		},
		"Replay a kickoff. Spawns a bot that replays the same recording.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(SAVE_COMMAND,
		[this](std::vector<std::string> args)
		{
			if (!pluginEnabled) return;

			this->saveRecording();
		},
		"Save the last kickoff. Recordings are saved automatically.",
		PERMISSION_FREEPLAY
	);
}

void KickoffPractice::hookEvents()
{
	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.Car_TA.SetVehicleInput",
		[this](CarWrapper car, void* params, std::string eventName)
		{
			if (!this->shouldExecute()) return;
			if (!params) return;

			ControllerInput* input = static_cast<ControllerInput*>(params);
			this->onVehicleInput(car, input);
		}
	);

	gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function TAGame.Car_TA.OnHitBall",
		[this](CarWrapper car, void* params, std::string eventname)
		{
			if (!this->shouldExecute()) return;

			if (this->kickoffState == KickoffState::started && !this->isBot(car))
				this->speedFlipTrainer->OnBallHit(car);

			if (this->kickoffState == KickoffState::started)
			{
				auto timeout = this->timeAfterBackToNormal;
				if (auto server = gameWrapper->GetCurrentGameState())
					timeout /= server.GetGameSpeed();

				this->setTimeoutChecked(
					timeout,
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
		}
	);

	gameWrapper->HookEvent(
		"Function Engine.Actor.SpawnInstance",
		[this](...)
		{
			if (!this->shouldExecute()) return;

			ServerWrapper server = gameWrapper->GetCurrentGameState();
			if (!server) return;
			if (!currentKickoffIndex.has_value()) return;

			for (auto car : server.GetCars())
			{
				if (!this->isBot(car)) continue;

				auto boost = car.GetBoostComponent();
				if (!boost) continue;

				Vector  locationBot = Utils::getKickoffLocation(this->currentKickoffPosition, KickoffSide::Orange);
				Rotator rotationBot = Utils::getKickoffRotation(this->currentKickoffPosition, KickoffSide::Orange);

				car.SetLocation(locationBot);
				car.SetCarRotation(rotationBot);
				car.Stop();
				// To disable the bot moving by itself we would call `car.GetAIController().DoNothing()` here.
				// But then the `SetVehicleInput` hook would not fire for the bot.
				// So we don't disable the controller, but overwrite the inputs inside the hook.

				applyBoostSettings(boost, INITIAL_BOOST_SETTINGS);
				boost.SetCurrentBoostAmount(INITIAL_BOOST_AMOUNT);

				auto& settings = loadedKickoffs[*currentKickoffIndex].settings;
				car.GetPRI().SetUserCarPreferences(settings.DodgeInputThreshold, settings.SteeringSensitivity, settings.AirControlSensitivity);
			}
		}
	);

	gameWrapper->HookEventPost(
		"Function GameEvent_Soccar_TA.ReplayPlayback.BeginState",
		[this](...) { this->isInGoalReplay = true; }
	);
	gameWrapper->HookEventPost(
		"Function GameEvent_Soccar_TA.ReplayPlayback.EndState",
		[this](...) { this->isInGoalReplay = false; }
	);

	gameWrapper->HookEventPost(
		// Called at the beginning/reset of freeplay and when a kickoff starts.
		"Function GameEvent_Soccar_TA.Countdown.EndState",
		[this](...)
		{
			if (!this->shouldExecute()) return;

			this->recordBoostSettings();
			this->reset();
		}
	);
	gameWrapper->HookEvent(
		// Called when resetting freeplay.
		"Function TAGame.PlayerController_TA.PlayerResetTraining",
		[this](...)
		{
			// Allow to break out of auto-restart by resetting freeplay.
			if (this->autoRestart && this->kickoffState != KickoffState::nothing)
				return;
			if (this->restartOnTrainingReset)
				this->start();
		}
	);
}

void KickoffPractice::registerDrawables()
{
	gameWrapper->RegisterDrawable(
		[this](CanvasWrapper canvas)
		{
			if (!shouldExecute()) return;
			if (gameWrapper->IsPaused()) return;
			if (kickoffState == KickoffState::nothing) return;

			this->renderIndicator(canvas);
			this->speedFlipTrainer->RenderMeters(canvas);
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
		std::vector<int> suitableKickoffIndices;
		for (int index = 0; index < loadedKickoffs.size(); index++)
		{
			auto& kickoff = loadedKickoffs[index];

			bool isSuitable = kickoff.isActive;

			if (this->positionOverride.has_value())
				isSuitable = isSuitable && kickoff.position == *this->positionOverride;
			else
				isSuitable = isSuitable && this->activePositions.contains(kickoff.position);

			if (isSuitable)
				suitableKickoffIndices.push_back(index);
		}

		if (suitableKickoffIndices.empty())
		{
			LOG("No suitable kickoff recording found! Make sure there are active kickoffs for your selected positions.");
			return;
		}

		this->setCurrentKickoffIndex(suitableKickoffIndices[rand() % suitableKickoffIndices.size()]);
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
	Vector  locationPlayer = Utils::getKickoffLocation(this->currentKickoffPosition, playerSide);
	Rotator rotationPlayer = Utils::getKickoffRotation(this->currentKickoffPosition, playerSide);

	if (this->mode != KickoffMode::Recording && currentKickoffIndex.has_value())
	{
		auto carBody = loadedKickoffs[*currentKickoffIndex].carBody;
		server.SpawnBot(carBody, BOT_CAR_NAME);
	}

	player.SetLocation(locationPlayer);
	player.SetRotation(rotationPlayer);
	player.Stop();

	KickoffPractice::applyBoostSettings(boost, INITIAL_BOOST_SETTINGS);
	boost.SetCurrentBoostAmount(INITIAL_BOOST_AMOUNT);

	// Reset boost pickups a few frames in, because moving the player can cause picking up boost.
	// Moving the player isn't done instantly, but takes a few frames.
	this->setTimeoutChecked(
		12 * gameWrapper->GetEngine().GetBulletFixedDeltaTime(),
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

		this->speedFlipTrainer->OnVehicleInput(player, input);
	}
}

std::optional<ControllerInput> KickoffPractice::getRecordedInput()
{
	if (currentKickoffIndex == std::nullopt)
		return std::nullopt;

	auto& inputs = loadedKickoffs[*currentKickoffIndex].inputs;
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
	this->speedFlipTrainer->Reset();
}

void KickoffPractice::clearLoadedKickoffs()
{
	loadedKickoffs.clear();
	kickoffIndexByName.clear();
	kickoffIndexByPosition.clear();
	setCurrentKickoffIndex(std::nullopt);
}
void KickoffPractice::loadKickoff(RecordedKickoff& kickoff)
{
	loadedKickoffs.push_back(kickoff);
	auto index = loadedKickoffs.size() - 1;
	kickoffIndexByName[kickoff.name] = index;
	kickoffIndexByPosition[kickoff.position].push_back(index);
}
void KickoffPractice::renameKickoff(std::string oldName, std::string newName)
{
	if (!kickoffIndexByName.contains(oldName)) return;
	if (kickoffIndexByName.contains(newName)) return;

	auto index = kickoffIndexByName[oldName];
	loadedKickoffs[index].name = newName;

	kickoffIndexByName.erase(oldName);
	kickoffIndexByName[newName] = index;
}
void KickoffPractice::unloadKickoff(std::string name)
{
	if (!kickoffIndexByName.contains(name)) return;

	auto index = kickoffIndexByName[name];
	loadedKickoffs.erase(loadedKickoffs.begin() + index);

	if (currentKickoffIndex.has_value())
	{
		if (index == currentKickoffIndex) setCurrentKickoffIndex(std::nullopt);
		else if (index < currentKickoffIndex) setCurrentKickoffIndex(*currentKickoffIndex - 1);
	}

	// Re-create the lookup maps because of index shift.
	kickoffIndexByName.clear();
	kickoffIndexByPosition.clear();
	for (int i = 0; i < loadedKickoffs.size(); i++)
	{
		auto& kickoff = loadedKickoffs[i];
		kickoffIndexByName[kickoff.name] = i;
		kickoffIndexByPosition[kickoff.position].push_back(i);
	}
}
void KickoffPractice::setCurrentKickoffIndex(std::optional<int> index)
{
	if (index == std::nullopt || 0 > *index || *index >= loadedKickoffs.size())
	{
		currentKickoffIndex = std::nullopt;
		return;
	}

	currentKickoffIndex = index;
	currentKickoffPosition = loadedKickoffs[*index].position;
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

	kickoffStorage->saveRecording(kickoff);
	loadKickoff(kickoff);
	kickoffStorage->saveActiveKickoffs(loadedKickoffs);
}

std::string KickoffPractice::getNewRecordingName() const
{
	std::string timestamp = Utils::getCurrentTimestamp();
	std::string kickoffName = Utils::getKickoffPositionName(this->currentKickoffPosition);

	return timestamp + " " + kickoffName;
}

void KickoffPractice::readKickoffsFromFile()
{
	clearLoadedKickoffs();

	for (auto& kickoff : kickoffStorage->readRecordings())
		loadKickoff(kickoff);
}

void KickoffPractice::renameKickoffFile(std::string oldName, std::string newName, std::function<void()> onSuccess)
{
	if (newName.empty()) return;
	if (!kickoffIndexByName.contains(oldName)) return;
	if (kickoffIndexByName.contains(newName)) return;

	if (!kickoffStorage->renameKickoffFile(oldName, newName)) return;

	renameKickoff(oldName, newName);

	onSuccess();
}

void KickoffPractice::deleteKickoffFile(std::string name, std::function<void()> onSuccess)
{
	if (!kickoffIndexByName.contains(name))
		return;

	if (!kickoffStorage->deleteKickoffFile(name)) return;

	unloadKickoff(name);

	onSuccess();
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

std::optional<KickoffPosition> KickoffPractice::parseKickoffArg(std::string arg)
{
	const int kickoffNumber = stoi(arg);
	if (kickoffNumber < 1 || kickoffNumber > 5)
	{
		LOG("The kickoff number argument should be between 1 and 5 (included).");
		return std::nullopt;
	}
	return Utils::fromInt(kickoffNumber - 1);
}
std::string KickoffPractice::getKickoffArg(KickoffPosition position)
{
	auto kickoffNumber = Utils::toInt(position);
	return std::to_string(kickoffNumber + 1);
}

std::string KickoffPractice::getActivePositionsMask()
{
	std::string mask = "00000";
	for (auto position : activePositions)
	{
		auto index = Utils::toInt(position);
		if (index < mask.size()) mask[index] = '1';
	}
	return mask;
}
void KickoffPractice::setActivePositionFromMask(std::string mask)
{
	activePositions.clear();

	for (int index = 0; index < 5; index++)
		if (index < mask.size() && mask.at(index) == '1')
			activePositions.insert(Utils::fromInt(index));
}

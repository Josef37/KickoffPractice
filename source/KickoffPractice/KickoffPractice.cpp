#include "pch.h"
#include "KickoffPractice.h"

BAKKESMOD_PLUGIN(KickoffPractice, "Kickoff Practice", plugin_version, PLUGINTYPE_FREEPLAY);

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

static const std::string PLUGIN_FOLDER = "kickoffpractice";
static const std::string BOT_CAR_NAME = "Kickoff Bot";

void KickoffPractice::onLoad()
{
	_globalCvarManager = cvarManager;

	// initialize the random number generator seed
	srand((int)time(0));

	persistentStorage = std::make_shared<PersistentStorage>(this, "kickoffpractice", true, true);

	speedFlipTrainer = std::make_unique<SpeedFlipTrainer>(
		gameWrapper,
		cvarManager,
		[this]() { return showSpeedFlipTrainer && shouldExecute(); }
	);

	kickoffStorage = std::make_unique<KickoffStorage>(
		gameWrapper->GetDataFolder() / PLUGIN_FOLDER
	);

	kickoffLoader = std::make_unique<KickoffLoader>();

	registerCvars();

	registerCommands();

	// Only enable functionality in freeplay.
	gameWrapper->HookEventPost(
		"Function TAGame.GameEvent_Soccar_TA.OnInit",
		[this](...) { load(); }
	);
	gameWrapper->HookEventPost(
		"Function TAGame.GameEvent_Soccar_TA.Destroyed",
		[this](...) { unload(); }
	);
}

void KickoffPractice::registerCvars()
{
	// TODO: Add description. Store cvar, title and description in variable to use it in UI, too.
	persistentStorage->RegisterPersistentCvar(CVAR_ENABLED, "1").addOnValueChanged([this](std::string oldValue, CVarWrapper cvar)
		{
			pluginEnabled = cvar.getBoolValue();

			gameWrapper->Execute([this](...)
				{
					if (pluginEnabled) load();
					else unload();
				});
		});

	persistentStorage->RegisterPersistentCvar(CVAR_RESTART_ON_RESET, "1")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { restartOnTrainingReset = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_AUTO_RESTART, "1")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { autoRestart = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_SHOW_INDICATOR, "1")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { showIndicator = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_SPEEDFLIP_TRAINER, "1")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { showSpeedFlipTrainer = cvar.getBoolValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_BACK_TO_NORMAL, "0.5", "", true, true, 0.0f)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { timeAfterBackToNormal = cvar.getFloatValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_COUNTDOWN_LENGTH, "3", "", true, true, 1.0f)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { countdownLength = cvar.getIntValue(); });

	persistentStorage->RegisterPersistentCvar(CVAR_ACTIVE_POSITIONS, getActivePositionsMask())
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { setActivePositionFromMask(cvar.getStringValue()); });


	speedFlipTrainer->RegisterCvars(persistentStorage);
}

void KickoffPractice::registerCommands()
{
	cvarManager->registerNotifier(TRAIN_COMMAND,
		[this](std::vector<std::string> args)
		{
			if (!shouldExecute(true)) return;

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
			if (!shouldExecute(true)) return;

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
			if (!shouldExecute(true)) return;

			this->mode = KickoffMode::Replaying;

			if (args.size() < 2)
			{
				LOG("Kickoff name argument expected for replaying.");
				return;
			}
			auto& kickoffName = args[1];
			auto kickoff = kickoffLoader->findKickoffByName(kickoffName);
			if (!kickoff)
			{
				LOG("No kickoff found with the specified name: {}", kickoffName);
				return;
			}
			setCurrentKickoff(kickoff);

			this->start();
		},
		"Replay a kickoff. Spawns a bot that replays the same recording.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(REPEAT_COMMAND,
		[this](std::vector<std::string> args)
		{
			if (!shouldExecute(true)) return;

			this->reset();
			this->start();
		},
		"Repeats the last train/record/replay command.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(RESET_COMMAND,
		[this](std::vector<std::string> args)
		{
			if (!shouldExecute(true)) return;

			this->reset();
		},
		"Reset the plugin and game state.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(SAVE_COMMAND,
		[this](std::vector<std::string> args)
		{
			// Always allow, because we can interact with cvars and settings.

			this->saveLastAttempt();
		},
		"Save the last kickoff. Recordings are saved automatically.",
		PERMISSION_FREEPLAY
	);

	cvarManager->registerNotifier(SELECT_COMMAND,
		[this](std::vector<std::string> args)
		{
			// Always allow, because we can interact with cvars and settings.

			if (args.size() < 2)
			{
				LOG("Kickoff name argument required.");
				return;
			}

			auto& kickoffName = args[1];
			auto kickoff = kickoffLoader->findKickoffByName(kickoffName);
			if (!kickoff)
			{
				LOG("No kickoff found with the specified name: {}", kickoffName);
				return;
			}

			kickoff->isActive = args.size() > 2
				? args[2] == "1"
				: !kickoff->isActive;
		},
		"Selects a kickoff for training.\n"
		"Arguments: <kickoff name> <(optional) state - 0:disable, 1:enable, missing:toggle>.",
		PERMISSION_FREEPLAY
	);
}

void KickoffPractice::load()
{
	if (!pluginEnabled) return;
	if (!gameWrapper->IsInFreeplay()) return;

	if (loaded) return;
	loaded = true;

	LOG("Loading plugin...");

	hookEvents();
	resetPluginState();
	if (kickoffLoader->getKickoffs().empty()) readKickoffsFromDisk();
	determineGameMode();
}

void KickoffPractice::unload()
{
	if (!loaded) return;
	loaded = false;

	LOG("Unloading plugin...");

	if (gameWrapper->IsInFreeplay() && kickoffState != KickoffState::Nothing)
		resetGameState();

	unhookEvents();
	gameMode = std::nullopt;
}

void KickoffPractice::hookEvents()
{
	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.Car_TA.SetVehicleInput",
		[this](CarWrapper car, void* params, std::string eventName)
		{
			if (!this->shouldExecute()) return;
			if (!params) return;

			// For each physics frame the hooks seem to be called in the following order:
			// - "SetVehicleInput" for player (one call)
			// - "EventPostPhysicsStep"
			// - "SetVehicleInput" for bot (multiple calls, depending on game speed)
			// To process everything beforehand, we use the player "SetVehicleInput" hook.
			if (!isBot(car))
				onPhysicsFrame();

			ControllerInput* input = static_cast<ControllerInput*>(params);
			this->onVehicleInput(car, input);
		}
	);

	gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function TAGame.Car_TA.OnHitBall",
		[this](CarWrapper car, void* params, std::string eventName)
		{
			if (!this->shouldExecute()) return;

			if (this->kickoffState == KickoffState::Started && !this->isBot(car))
				this->speedFlipTrainer->OnBallHit(car);

			if (this->kickoffState == KickoffState::Started)
			{
				auto timeout = this->timeAfterBackToNormal;
				if (auto server = gameWrapper->GetCurrentGameState())
					timeout /= server.GetGameSpeed();

				auto kickoffCounterOnHit = kickoffCounter;

				gameWrapper->SetTimeout(
					[this, kickoffCounterOnHit](...)
					{
						if (!this->shouldExecute()) return;
						if (this->kickoffCounter != kickoffCounterOnHit) return;
						if (this->kickoffState != KickoffState::Started) return;

						if (this->mode == KickoffMode::Recording)
							this->saveLastAttempt();

						this->reset();

						if (this->autoRestart)
							this->start();
					},
					timeout
				);
			}
		}
	);

	gameWrapper->HookEventPost(
		// This hook is called after spawning or destroying a bot and setting its properties.
		// Then we can safely set our own properties without them being overwritten.
		"Function TAGame.GameEvent_Team_TA.UpdateBotCount",
		[this](...)
		{
			if (!spawnBotCalled) return;
			spawnBotCalled = false;

			if (!this->shouldExecute()) return;

			shouldSetupKickoff = true;
		}
	);

	gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function TAGame.Car_TA.Demolish",
		[this](CarWrapper car, void* params, std::string eventName)
		{
			if (!shouldExecute()) return;
			if (kickoffState == KickoffState::Nothing) return;
			
			// Respawn all cars on demo - also bots.
			// If we didn't, the bot could respawn for the next shot a few seconds later.
			car.RespawnInPlace();
		}
	);

	gameWrapper->HookEventPost(
		"Function TAGame.GameEvent_Soccar_TA.OnBallSpawned",
		[this](...) { determineGameMode(); }
	);

	gameWrapper->HookEvent(
		"Function TAGame.Ball_TA.OnHitGoal",
		[this](...)
		{
			this->isInGoalReplay = true;

			if (this->kickoffState == KickoffState::Started)
			{
				if (this->mode == KickoffMode::Recording)
					this->saveLastAttempt();

				this->reset();
			}
		}
	);
	gameWrapper->HookEvent(
		"Function GameEvent_Soccar_TA.ReplayPlayback.BeginState",
		[this](...) { this->isInGoalReplay = true; }
	);
	gameWrapper->HookEventPost(
		"Function GameEvent_Soccar_TA.ReplayPlayback.EndState",
		[this](...) { this->isInGoalReplay = false; }
	);
	gameWrapper->HookEventPost(
		// Called at the beginning/reset of freeplay (or in-game kickoffs).
		"Function GameEvent_Soccar_TA.Countdown.EndState",
		[this](...) { this->isInGoalReplay = false; }
	);

	gameWrapper->HookEventPost(
		// Called when resetting freeplay.
		"Function TAGame.PlayerController_TA.PlayerResetTraining",
		[this](...)
		{
			if (!shouldExecute()) return;

			// Always break out of the current kickoff.
			auto shouldRestart = restartOnTrainingReset && kickoffState == KickoffState::Nothing;

			reset();

			if (shouldRestart)
				start();
		}
	);

	gameWrapper->RegisterDrawable(
		[this](CanvasWrapper canvas)
		{
			if (!shouldExecute()) return;
			if (gameWrapper->IsPaused()) return;
			if (kickoffState == KickoffState::Nothing) return;

			this->renderIndicator(canvas);
			this->speedFlipTrainer->RenderMeters(canvas);
		}
	);
}

void KickoffPractice::unhookEvents()
{
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
	gameWrapper->UnhookEventPost("Function TAGame.Car_TA.OnHitBall");
	gameWrapper->UnhookEventPost("Function TAGame.GameEvent_Team_TA.UpdateBotCount");
	gameWrapper->UnhookEventPost("Function TAGame.Car_TA.Demolish");
	gameWrapper->UnhookEventPost("Function TAGame.GameEvent_Soccar_TA.OnBallSpawned");
	gameWrapper->UnhookEvent("Function TAGame.Ball_TA.OnHitGoal");
	gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.BeginState");
	gameWrapper->UnhookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.EndState");
	gameWrapper->UnhookEventPost("Function GameEvent_Soccar_TA.Countdown.EndState");
	gameWrapper->UnhookEventPost("Function TAGame.PlayerController_TA.PlayerResetTraining");
	gameWrapper->UnregisterDrawables();
}

void KickoffPractice::determineGameMode()
{
	auto server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	auto ball = server.GetBall();
	if (!ball) return;

	gameMode = Utils::determineGameMode(ball);
}

void KickoffPractice::onUnload()
{
	unload();
}

bool KickoffPractice::shouldExecute(bool log)
{
	if (!pluginEnabled)
	{
		if (log) LOG("Plugin disabled. Enable at the top of the settings or set `{}`.", CVAR_ENABLED);
		return false;
	}
	if (!gameWrapper->IsInFreeplay())
	{
		if (log) LOG("Plugin only active in freeplay.");
		return false;
	}
	if (this->isInGoalReplay)
	{
		if (log) LOG("Plugin not active during replay.");
		return false;
	}
	if (this->gameMode == std::nullopt)
	{
		if (log) LOG("Could not determine game-mode.");
		return false;
	}

	return true;
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
		std::vector<std::shared_ptr<RecordedKickoff>> suitableKickoffs;
		for (auto& kickoff : kickoffLoader->getKickoffs(gameMode))
		{
			bool isSuitable = kickoff->isActive;

			if (this->positionOverride.has_value())
				isSuitable = isSuitable && kickoff->position == *this->positionOverride;
			else
				isSuitable = isSuitable && this->activePositions.contains(kickoff->position);

			if (isSuitable)
				suitableKickoffs.push_back(kickoff);
		}

		if (suitableKickoffs.empty())
		{
			LOG("No suitable kickoff recording found! Make sure there are active kickoffs for your selected positions.");
			return;
		}

		// TODO: Better random selection.
		auto& newKickoff = suitableKickoffs[rand() % suitableKickoffs.size()];
		setCurrentKickoff(newKickoff);
	}

	if (this->mode == KickoffMode::Recording)
	{
		// When recording we don't need a bot to spawn.
		shouldSetupKickoff = true;
	}
	else
	{
		auto server = gameWrapper->GetCurrentGameState();
		if (!server) return;
		auto currentKickoff = kickoffLoader->getCurrentKickoff();
		if (!currentKickoff) return;

		auto carBody = currentKickoff->carBody;
		server.SpawnBot(carBody, BOT_CAR_NAME);
		spawnBotCalled = true;
	}
}

void KickoffPractice::onPhysicsFrame()
{
	setupKickoff();
	doCountdown();
	updateBall();
}

void KickoffPractice::setupKickoff()
{
	auto server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	if (!shouldSetupKickoff) return;
	shouldSetupKickoff = false;

	for (auto car : server.GetCars())
	{
		if (isBot(car)) setupBot(car);
		else			setupPlayer(car);
	}

	if (auto ball = server.GetBall())
	{
		RBState ballState;
		ballState.Location = Vector(0, 0, ball.GetRadius());
		ball.SetPhysicsState(ballState);
	}

	server.ResetPickups();

	kickoffState = KickoffState::WaitingToStart;

	initCountdown();
}
void KickoffPractice::setupPlayer(CarWrapper player)
{
	if (!gameMode) return;

	KickoffSide playerSide = this->mode == KickoffMode::Recording ? KickoffSide::Orange : KickoffSide::Blue;
	Vector  locationPlayer = Utils::getKickoffLocation(this->currentKickoffPosition, playerSide, *gameMode);
	Rotator rotationPlayer = Utils::getKickoffRotation(this->currentKickoffPosition, playerSide, *gameMode);

	RBState playerState;
	playerState.Location = locationPlayer;
	playerState.Quaternion = RotatorToQuat(rotationPlayer);
	player.SetPhysicsState(playerState);
	player.SetbDriving(false);

	if (BoostWrapper boost = player.GetBoostComponent())
	{
		applyBoostSettings(boost, Utils::getInitialBoostSettings(*gameMode));
		boost.SetCurrentBoostAmount(Utils::getInitialBoostAmount(*gameMode));
	}
}
void KickoffPractice::setupBot(CarWrapper bot)
{
	if (!gameMode) return;

	Vector  locationBot = Utils::getKickoffLocation(this->currentKickoffPosition, KickoffSide::Orange, *gameMode);
	Rotator rotationBot = Utils::getKickoffRotation(this->currentKickoffPosition, KickoffSide::Orange, *gameMode);

	RBState botState;
	botState.Location = locationBot;
	botState.Quaternion = RotatorToQuat(rotationBot);
	bot.SetPhysicsState(botState);
	// We need to set this flag, otherwise the bot will be slightly offset when the kickoff starts,
	// even when we overwrite all inputs.
	bot.SetbDriving(false);

	// To disable the bot moving by itself we would call `car.GetAIController().DoNothing()` here.
	// But then the `SetVehicleInput` hook would not fire for the bot.
	// So we don't disable the controller, but overwrite the inputs inside the hook.

	if (auto boost = bot.GetBoostComponent())
	{
		applyBoostSettings(boost, Utils::getInitialBoostSettings(*gameMode));
		boost.SetCurrentBoostAmount(Utils::getInitialBoostAmount(*gameMode));
	}

	if (auto currentKickoff = kickoffLoader->getCurrentKickoff())
	{
		auto& settings = currentKickoff->settings;
		bot.GetPRI().SetUserCarPreferences(settings.DodgeInputThreshold, settings.SteeringSensitivity, settings.AirControlSensitivity);
	}
	else
		LOG("Unable to set sensitivities for bot from recording...");
}

void KickoffPractice::initCountdown()
{
	auto engine = gameWrapper->GetEngine();
	if (!engine) return;

	int seconds = countdownLength;
	if (mode == KickoffMode::Replaying) seconds = 1;

	int framesLeft = seconds * lroundf(engine.GetPhysicsFramerate());
	startingFrame = engine.GetPhysicsFrame() + framesLeft;
}
void KickoffPractice::doCountdown()
{
	if (this->kickoffState != KickoffState::WaitingToStart) return;

	auto server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	auto engine = gameWrapper->GetEngine();
	if (!engine) return;

	int framesLeft = startingFrame - engine.GetPhysicsFrame();
	int frameRate = lroundf(engine.GetPhysicsFramerate());

	if (framesLeft == 0)
	{
		server.SendGoMessage(gameWrapper->GetPlayerController());

		kickoffState = KickoffState::Started;
		initNewRecording();
	}
	else if (framesLeft % frameRate == 0)
	{
		auto seconds = framesLeft / frameRate;
		server.SendCountdownMessage(seconds, gameWrapper->GetPlayerController());
	}
}

void KickoffPractice::updateBall()
{
	auto server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	auto ball = server.GetBall();
	if (!ball) return;

	if (!gameMode) return;

	if (kickoffState == KickoffState::WaitingToStart)
	{
		// In hoops freeplay the ball will be launched some 200 ms after resetting freeplay.
		// We want to keep it on the ground until kickoff start.
		RBState ballState;
		ballState.Location = Utils::getKickoffBallLocation(*gameMode);
		ball.SetPhysicsState(ballState);
	}

	if (kickoffState == KickoffState::Started)
	{
		// 30 ticks after kickoff start was determined by measurement.
		auto launchFrame = startingFrame + 30;

		auto engine = gameWrapper->GetEngine();
		if (!engine) return;
		auto currentFrame = engine.GetPhysicsFrame();

		if (launchFrame == currentFrame)
		{
			RBState ballState;
			ballState.Location = Utils::getKickoffBallLocation(*gameMode);
			ballState.LinearVelocity = Utils::getKickoffBallVelocity(*gameMode);
			ball.SetPhysicsState(ballState);
		}
	}
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
	// TODO: When dodging or colliding before restarting, the position is slightly offset...
	if (this->isBot(car))
	{
		auto& bot = car;

		*input = EMPTY_INPUT;

		// Don't set the flag to `false` when waiting, because the bot spawns later
		// and updating the position wouldn't work anymore with the flag set.
		if (this->kickoffState != KickoffState::WaitingToStart)
			bot.SetbDriving(true);

		if (this->kickoffState != KickoffState::Started)
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

		if (this->kickoffState != KickoffState::WaitingToStart)
			player.SetbDriving(true);

		// If the player is holding boost when starting training, it won't stop consuming boost.
		// Disabling was quite messy (with `PlayerController::ToggleBoost` or `BoostComponent::SetbActive`),
		// which resulted in weird game states. So we just fill the tank every tick.
		if (auto boost = player.GetBoostComponent())
			if (this->kickoffState == KickoffState::WaitingToStart && gameMode.has_value())
				boost.SetCurrentBoostAmount(Utils::getInitialBoostAmount(*gameMode));

		if (this->kickoffState != KickoffState::Started)
			return;

		if (this->mode == KickoffMode::Replaying)
		{
			auto recordedInput = getRecordedInput();
			if (recordedInput.has_value())
				*input = *recordedInput;
		}

		recordInput(*input);

		this->speedFlipTrainer->OnVehicleInput(player, input);
	}
}

std::optional<ControllerInput> KickoffPractice::getRecordedInput()
{
	auto currentKickoff = kickoffLoader->getCurrentKickoff();
	if (!currentKickoff)
		return std::nullopt;

	auto& inputs = currentKickoff->inputs;
	auto currentFrame = gameWrapper->GetEngine().GetPhysicsFrame();
	auto tick = currentFrame - this->startingFrame;

	if (0 > tick || tick >= inputs.size())
		return std::nullopt;

	return inputs[tick];
}

void KickoffPractice::reset()
{
	resetPluginState();
	resetGameState();
}
void KickoffPractice::resetPluginState()
{
	this->kickoffState = KickoffState::Nothing;
	this->shouldSetupKickoff = false;
	this->speedFlipTrainer->Reset();
}
void KickoffPractice::resetGameState()
{
	this->removeBots();
	this->resetBoostSettings();
}

void KickoffPractice::setCurrentKickoff(std::shared_ptr<RecordedKickoff> kickoff)
{
	kickoffLoader->setCurrentKickoff(kickoff);
	if (kickoff) currentKickoffPosition = kickoff->position;
}

void KickoffPractice::initNewRecording()
{
	lastAttempt = std::make_shared<RecordedKickoff>();
	lastAttempt->name = getNewRecordingName();
	lastAttempt->position = this->currentKickoffPosition;
	lastAttempt->carBody = gameWrapper->GetLocalCar().GetLoadoutBody();
	lastAttempt->settings = gameWrapper->GetSettings().GetGamepadSettings();
	lastAttempt->gameMode = this->gameMode.value_or(GameMode::Soccar);
	lastAttempt->isActive = true;
}

std::string KickoffPractice::getNewRecordingName() const
{
	std::string timestamp = Utils::getCurrentTimestamp();
	std::string positionName = Utils::getKickoffPositionName(currentKickoffPosition);
	std::string gameModeName = Utils::getGameModeName(gameMode.value_or(GameMode::Soccar));

	return gameModeName + " - " + positionName + " - " + timestamp;
}

void KickoffPractice::recordInput(ControllerInput& input)
{
	lastAttempt->inputs.push_back(input);
}

void KickoffPractice::saveLastAttempt()
{
	if (!lastAttempt || lastAttempt->inputs.empty())
	{
		LOG("Nothing recorded.");
		return;
	}
	LOG("Saving... Ticks recorded: {}", lastAttempt->inputs.size());

	kickoffStorage->saveRecording(lastAttempt);
	kickoffLoader->loadKickoff(lastAttempt);
	kickoffStorage->saveActiveKickoffs(kickoffLoader->getKickoffs());
}

void KickoffPractice::readKickoffsFromDisk()
{
	auto kickoffs = kickoffStorage->readRecordings();

	kickoffLoader->clearLoadedKickoffs();
	kickoffLoader->loadKickoffs(kickoffs);
}

void KickoffPractice::renameKickoffFile(std::string oldName, std::string newName, std::function<void()> onSuccess)
{
	if (newName.empty()) return;
	if (!kickoffLoader->findKickoffByName(oldName)) return;
	if (kickoffLoader->findKickoffByName(newName)) return;

	if (!kickoffStorage->renameKickoffFile(oldName, newName)) return;

	kickoffLoader->renameKickoff(oldName, newName);

	onSuccess();
}

void KickoffPractice::deleteKickoffFile(std::string name, std::function<void()> onSuccess)
{
	if (!kickoffLoader->findKickoffByName(name))
		return;

	if (!kickoffStorage->deleteKickoffFile(name)) return;

	kickoffLoader->unloadKickoff(name);

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

	// To avoid the lightning that shows on `server.RemovePlayer()` we call `car.Destroy()` first.
	// After `car.Destroy()` `car.GetAIController()` wouldn't work, so we have to store it beforehand.
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
	if (this->kickoffState != KickoffState::Nothing) return;

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
	return Utils::positionFromInt(kickoffNumber - 1);
}
std::string KickoffPractice::getKickoffArg(KickoffPosition position)
{
	auto kickoffNumber = Utils::positionToInt(position);
	return std::to_string(kickoffNumber + 1);
}

std::string KickoffPractice::getActivePositionsMask()
{
	std::string mask = "00000";
	for (auto position : activePositions)
	{
		auto index = Utils::positionToInt(position);
		if (index < mask.size()) mask[index] = '1';
	}
	return mask;
}
void KickoffPractice::setActivePositionFromMask(std::string mask)
{
	activePositions.clear();

	for (int index = 0; index < 5; index++)
		if (index < mask.size() && mask.at(index) == '1')
			activePositions.insert(Utils::positionFromInt(index));
}

#include "pch.h"
#include "KickoffPractice.h"

BAKKESMOD_PLUGIN(KickoffPractice, "Kickoff Practice", plugin_version, PLUGINTYPE_FREEPLAY);

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
namespace fs = std::filesystem;

static const BoostSettings INITIAL_BOOST_SETTINGS = BoostSettings{
	.UnlimitedBoostRefCount = 0,
	.CurrentBoostAmount = 0.333f,
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
	this->isInReplay = false;
	this->currentInputIndex = 0;
	this->isRecording = false;
	this->rotationBot = Rotator(0, 0, 0);
	this->locationBot = Vector(0, 0, 0);
	this->tickCounter = 0;
	this->kickoffState = KickoffState::nothing;
	this->botJustSpawned = false;
	this->currentKickoffIndex = 0;
	this->botCarID = 0;
	this->selectedCarUI = 0;
	srand((int)time(0)); // initialize the random number generator seed

	this->configPath = gameWrapper->GetDataFolder() / PLUGIN_FOLDER;
	if (!fs::exists(this->configPath) || !fs::is_directory(this->configPath))
		if (!fs::create_directory(this->configPath))
			LOG("Can't create config directory in bakkesmod data folder");

	this->readConfigFile(this->configPath / CONFIG_FILE);

	// TODO: Extract creating folders.
	if (this->botKickoffFolder.empty()) this->botKickoffFolder = this->configPath / DEFAULT_BOT_FOLDER;
	if (!fs::exists(this->botKickoffFolder) || !fs::is_directory(this->botKickoffFolder))
		if (!fs::create_directory(this->botKickoffFolder))
			LOG("Can't create bot kickoff input directory in bakkesmod data folder");

	if (this->recordedKickoffFolder.empty()) this->recordedKickoffFolder = this->configPath / DEFAULT_RECORDING_FOLDER;
	if (!fs::exists(this->recordedKickoffFolder) || !fs::is_directory(this->recordedKickoffFolder))
		if (!fs::create_directory(this->recordedKickoffFolder))
			LOG("Can't create recorded inputs directory in bakkesmod data folder");

	this->readKickoffFiles();

	this->storeCarBodies();

	cvarManager->registerNotifier("kickoff_train",
		[this](std::vector<std::string> args)
		{
			// Use a timeout to start after other commands bound to the same button.
			this->setTimeoutChecked(0.1f, [this, args]() { this->start(args); });
		},
		"Practice kickoff",
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

			if (this->kickoffState == KickoffState::started)
			{
				this->setTimeoutChecked(
					this->timeAfterBackToNormal,
					[this]()
					{
						if (this->kickoffState != KickoffState::started)
							return;
						this->reset();
					}
				);
			}
		}
	);

	gameWrapper->HookEvent(
		"Function Engine.Actor.SpawnInstance",
		[this](std::string eventName)
		{
			if (!this->shouldExecute()) return;

			ServerWrapper server = gameWrapper->GetCurrentGameState();
			if (!server) return;
			// `SpawnInstance` gets called multiple times for the same car.
			// We make sure to execute the following code only once.
			if (!this->botJustSpawned) return;

			for (auto car : server.GetCars())
			{
				if (car.GetPRI().GetbBot())
				{
					car.SetLocation(this->locationBot);
					car.SetCarRotation(this->rotationBot);
					car.Stop();
					// To disable the bot moving by itself we would call `car.GetAIController().DoNothing()` here.
					// But then the `SetVehicleInput` hook would not fire for the bot.
					// So we don't disable the controller, but overwrite the inputs inside the hook.

					auto settings = this->loadedInputs[this->currentInputIndex].settings;
					car.GetPRI().SetUserCarPreferences(settings.DodgeInputThreshold, settings.SteeringSensitivity, settings.AirControlSensitivity);
				}
			}
			this->botJustSpawned = false;
		}
	);

	gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function TAGame.Ball_TA.OnHitGoal",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			this->isInReplay = true;
		}
	);
	gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function GameEvent_Soccar_TA.ReplayPlayback.EndState",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			this->isInReplay = false;
		}
	);
	gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function GameEvent_Soccar_TA.Countdown.EndState", // Called at the beginning/reset of freeplay.
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			if (!this->shouldExecute()) return;

			this->reset();
		}
	);
}

void KickoffPractice::onUnload()
{
	for (int i = 0; i < this->nbCarBody; i++)
	{
		delete[] this->carNames[i];
	}
	delete[] this->carNames;
}

bool KickoffPractice::shouldExecute()
{
	return pluginEnabled && gameWrapper->IsInFreeplay() && !this->isInReplay;
}

void KickoffPractice::setTimeoutChecked(float seconds, std::function<void()> callback)
{
	gameWrapper->SetTimeout(
		[this, callback](GameWrapper* _)
		{
			if (!this->shouldExecute())
				return;

			callback();
		},
		seconds
	);
}

/// args[1] = kickoff location (1-5)
/// args[2] = is recording? (bool or 0/1)
void KickoffPractice::start(std::vector<std::string> args)
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	this->recordBoostSettings();
	this->reset();

	if (args.size() >= 3)
	{
		this->isRecording = (args[2] == "1" || args[2] == "true");
	}
	if (!this->isRecording && this->loadedKickoffIndices.size() == 0)
	{
		LOG("No inputs selected");
		return;
	}

	// Determine the current kickoff index.
	if (args.size() >= 2)
	{
		const int kickoffNumber = stoi(args[1]);
		if (kickoffNumber < 1 || kickoffNumber > 5)
		{
			LOG("The kickoff number argument should be between 1 and 5 (included).");
			return;
		}
		this->currentKickoffIndex = kickoffNumber - 1;
		auto it = std::find(this->loadedKickoffIndices.begin(), this->loadedKickoffIndices.end(), this->currentKickoffIndex);
		if (it == this->loadedKickoffIndices.end() && !this->isRecording)
		{
			LOG("No input found for this kickoff");
			return;
		}
	}
	else
	{
		if (this->loadedKickoffIndices.size() == 0)
		{
			LOG("No inputs selected");
			return;
		}
		int kickoffNumber = (rand() % this->loadedKickoffIndices.size());
		this->currentKickoffIndex = this->loadedKickoffIndices[kickoffNumber];
	}

	this->currentInputIndex = this->getRandomKickoffForId(this->currentKickoffIndex);
	if (this->currentInputIndex == -1 && (!this->isRecording))
	{
		LOG("Error, no input found for this kickoff");
		return;
	}

	LOG("isRecording: {}", this->isRecording);

	KickoffSide playerSide = this->isRecording ? KickoffSide::Orange : KickoffSide::Blue;
	Vector locationPlayer = KickoffPractice::getKickoffLocation(this->currentKickoffIndex, playerSide);
	Rotator rotationPlayer = Rotator(0, std::lroundf(KickoffPractice::getKickoffYaw(this->currentKickoffIndex, playerSide) * CONST_RadToUnrRot), 0);
	this->locationBot = KickoffPractice::getKickoffLocation(this->currentKickoffIndex, KickoffSide::Orange);
	this->rotationBot = Rotator(0, std::lroundf(KickoffPractice::getKickoffYaw(this->currentKickoffIndex, KickoffSide::Orange) * CONST_RadToUnrRot), 0);
	if (!this->isRecording)
	{
		server.SpawnBot(this->botCarID, BOT_CAR_NAME);
		this->botJustSpawned = true;
	}

	CarWrapper player = gameWrapper->GetLocalCar();
	if (!player) return;
	player.SetLocation(locationPlayer);
	player.SetRotation(rotationPlayer);
	player.Stop();

	KickoffPractice::applyBoostSettings(player, INITIAL_BOOST_SETTINGS);

	// Reset boost pickups, because moving the player can cause picking up boost.
	this->setTimeoutChecked(
		0.1f,
		[this]()
		{
			if (auto server = gameWrapper->GetCurrentGameState())
				server.ResetPickups();
		}
	);

	BallWrapper ball = server.GetBall();
	if (!ball) return;
	ball.SetLocation(Vector(0, 0, ball.GetRadius()));
	ball.SetVelocity(Vector(0, 0, 0));
	ball.SetAngularVelocity(Vector(0, 0, 0), false);

	this->kickoffState = KickoffState::waitingToStart;

	startCountdown(
		3,
		[this]() { this->kickoffState = KickoffState::started; }
	);

	if (this->isRecording)
		LOG("Recording begins");
}

void KickoffPractice::startCountdown(int seconds, std::function<void()> onCompleted)
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	if (this->kickoffState != KickoffState::waitingToStart) return;

	if (seconds <= 0)
	{
		server.SendGoMessage(gameWrapper->GetPlayerController());
		onCompleted();

		return;
	}

	server.SendCountdownMessage(seconds, gameWrapper->GetPlayerController());

	// TODO: Verify the countdown is not delayed too much because the timeout might only be a lower bound.
	this->setTimeoutChecked(
		1.0f,
		[this, seconds, onCompleted]()
		{
			this->startCountdown(seconds - 1, onCompleted);
		}
	);
}

void KickoffPractice::onVehicleInput(CarWrapper car, ControllerInput* input)
{
	if (this->kickoffState == KickoffState::waitingToStart)
	{
		// Inputting steer when the car falls to the ground initially changes the yaw slightly (about 0.05 degrees).
		// This also works in-game! But we try to be as consistent as possible (also makes testing easier).
		// The side-effect is that the player can't wiggle his wheels when waiting... :(
		input->Steer = 0;
	}

	bool isBot = car.GetPRI().GetbBot();

	if (isBot)
	{
		auto& bot = car;

		bot.SetbDriving(this->kickoffState == KickoffState::started);

		if (this->kickoffState != KickoffState::started)
			return;

		auto& inputs = this->loadedInputs[this->currentInputIndex].inputs;

		// The Bot AI Controller somehow calls this functions twice per tick.
		// For now we just set the same input twice, the `tickCounter` incrementing twice per tick.
		auto tick = this->tickCounter / 2;

		if (tick < inputs.size())
		{
			ControllerInput loadedInput = inputs[tick];
			*input = loadedInput;
		}

		this->tickCounter += 1;
	}
	else
	{
		auto& player = car;

		player.SetbDriving(this->kickoffState != KickoffState::waitingToStart);

		if (this->kickoffState != KickoffState::started)
			return;

		if (this->isRecording)
		{
			this->recordedInputs.push_back(*input);
		}
	}
}

void KickoffPractice::reset()
{
	this->removeBots();

	if (this->isRecording)
	{
		const size_t numberOfInputs = this->recordedInputs.size();
		LOG("Recording ends. Ticks recorded : {}", numberOfInputs);

		auto time = std::time(nullptr);
		std::ostringstream oss;
		oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H-%M-%S");
		std::string timestamp = oss.str();

		std::string name = KickoffPractice::getKickoffName(this->currentKickoffIndex);
		std::string filename = name + " " + timestamp + FILE_EXT;

		std::ofstream inputFile(this->recordedKickoffFolder / filename);
		if (!inputFile.is_open())
		{
			LOG("ERROR : can't create recording file");
			return;
		}

		const GamepadSettings settings = gameWrapper->GetSettings().GetGamepadSettings();

		inputFile << settings.ControllerDeadzone
			<< "," << settings.DodgeInputThreshold
			<< "," << settings.SteeringSensitivity
			<< "," << settings.AirControlSensitivity
			<< "\n";

		for (int i = 0; i < numberOfInputs; i++)
		{
			const ControllerInput input = this->recordedInputs[i];

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
		this->recordedInputs.clear();
	}

	this->kickoffState = KickoffState::nothing;
	this->tickCounter = 0;
	this->currentKickoffIndex = 0;
	this->resetBoostSettings();
	this->isInReplay = false;
	this->isRecording = false;
}

int KickoffPractice::getRandomKickoffForId(int kickoffId)
{
	std::vector<int> indices;
	for (int i = 0; i < this->loadedInputs.size(); i++)
	{
		if (this->states[i] - 1 == kickoffId)
		{
			indices.push_back(i);
		}
	}
	if (indices.size() == 0)
		return -1;
	return indices[(rand() % indices.size())];
}

void KickoffPractice::removeBots()
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;

	for (auto car : server.GetCars())
	{
		if (car.GetPRI().GetbBot())
		{
			// To avoid the lightning that shows on `server.RemovePlayer()` we call `car.Destory()` first.
			// After `car.Destory()` `car.GetAIController()` wouldn't work, so we have to store it beforehand.
			// If we don't call `server.RemovePlayer()`, the bots will respawn on "Reset Ball".
			auto controller = car.GetAIController();
			car.Destroy();
			server.RemovePlayer(controller);
		}
	}
}

void KickoffPractice::writeConfigFile(std::wstring fileName)
{
	std::ofstream inputFile(fileName);
	if (!inputFile.is_open())
	{
		LOG("ERROR : can't create config file");
		return;
	}
	inputFile << this->botKickoffFolder.string() << "\n";
	inputFile << this->recordedKickoffFolder.string() << "\n";

	for (int i = 0; i < this->states.size(); i++)
	{
		inputFile << this->states[i] << "," << this->loadedInputs[i].name << "\n";
	}
	inputFile.close();
}

void KickoffPractice::readConfigFile(std::wstring fileName)
{
	std::vector<std::string> row;
	std::string line, word;
	std::string botFolder, recordFolder;
	std::fstream file(fileName, std::ios::in);
	if (file.is_open())
	{
		int i = 0;
		while (getline(file, line))
		{
			i++;
			row.clear();

			std::stringstream str(line);
			if (i == 1)
			{
				botFolder = line;
				continue;
			}
			if (i == 2)
			{
				recordFolder = line;
				continue;
			}
			while (getline(str, word, ','))
			{
				row.push_back(word);
			}
			if (row.size() != 2)
			{
				cvarManager->log("Error on line " + std::to_string(i));
				continue;
			}
			try
			{
				std::string name = row[1];
				int state = std::stoi(row[0]);
				if (state < 0 || state > 5) continue;
				for (int j = 0; j < this->loadedInputs.size(); j++)
				{
					if (this->loadedInputs[j].name == name)
					{
						this->states[j] = state;
					}
				}
			}
			catch (std::invalid_argument exception)
			{
				LOG("ERROR : invalid argument in config file\n{}", exception.what());
			}
			catch (std::out_of_range exception)
			{
				LOG("ERROR : number too big in config file \n{}", exception.what());
			}
		}
	}
	else
		cvarManager->log("Can't open the config file");

	if (botFolder.empty())
		this->botKickoffFolder = this->configPath / DEFAULT_BOT_FOLDER;
	else
		this->botKickoffFolder = botFolder;

	if (recordFolder.empty())
		this->recordedKickoffFolder = this->configPath / DEFAULT_RECORDING_FOLDER;
	else
		this->recordedKickoffFolder = recordFolder;
}

void KickoffPractice::readKickoffFiles()
{
	this->loadedInputs = std::vector<RecordedKickoff>();
	this->states = std::vector<int>();

	try
	{
		for (const auto& entry : fs::directory_iterator(this->botKickoffFolder))
		{
			if (entry.is_regular_file() && entry.path().extension() == FILE_EXT)
			{
				this->loadedInputs.push_back(this->readKickoffFile(entry.path().string(), entry.path().filename().string()));
				this->states.push_back(0);
			}
		}
	}
	catch (std::filesystem::filesystem_error const& ex)
	{
		LOG("ERROR : {}", ex.code().message());
	}
	this->readConfigFile(this->configPath / CONFIG_FILE);
	this->updateLoadedKickoffIndices();
}

RecordedKickoff KickoffPractice::readKickoffFile(std::string fileName, std::string kickoffName)
{
	RecordedKickoff kickoff;
	kickoff.name = kickoffName;

	std::vector<std::string> row;
	std::string line, word;
	std::vector<ControllerInput> currentInputs;

	std::fstream file(fileName, std::ios::in);
	if (file.is_open())
	{
		int i = 0;
		while (getline(file, line))
		{
			i++;
			row.clear();

			std::stringstream str(line);

			while (getline(str, word, ','))
			{
				row.push_back(word);
			}

			if (i == 1)
			{
				GamepadSettings settings{};

				if (row.size() == 4)
				{
					settings.ControllerDeadzone = std::stof(row[0]);
					settings.DodgeInputThreshold = std::stof(row[1]);
					settings.SteeringSensitivity = std::stof(row[2]);
					settings.AirControlSensitivity = std::stof(row[3]);

					kickoff.settings = settings;

					continue;
				}
				else
				{
					LOG("Error on line {} : size of {} instead of 4", i, row.size());
					LOG("Assuming old format without settings in first line...");

					settings.ControllerDeadzone = 0;
					settings.DodgeInputThreshold = 0;
					settings.SteeringSensitivity = 1;
					settings.AirControlSensitivity = 1;

					kickoff.settings = settings;
				}
			}

			ControllerInput input;
			if (row.size() != 12)
			{
				LOG("Error on line {} : size of {} instead of 12", i, row.size());
				continue;
			}
			try
			{
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

				currentInputs.push_back(input);
			}
			catch (std::invalid_argument exception)
			{
				LOG("ERROR : invalid argument in input file {}\n{}", fileName, exception.what());
			}
			catch (std::out_of_range exception)
			{
				LOG("ERROR : number too big in file {} \n{}", fileName, exception.what());
			}
		}
	}
	else
	{
		LOG("Can't open {}", fileName);
	}
	LOG("{} : {} lines loaded", fileName, currentInputs.size());

	kickoff.inputs = currentInputs;

	return kickoff;
}

void KickoffPractice::updateLoadedKickoffIndices()
{
	this->loadedKickoffIndices = std::vector<int>();
	for (int i = 0; i < this->states.size(); i++)
	{
		if (this->states[i] == 0) continue;
		int index = this->states[i] - 1;
		auto it = std::find(this->loadedKickoffIndices.begin(), this->loadedKickoffIndices.end(), index);
		if (it == this->loadedKickoffIndices.end()) // if we haven't found it
		{
			this->loadedKickoffIndices.push_back(index);
		}
	}
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
	settings.CurrentBoostAmount = boost.GetCurrentBoostAmount();
	settings.NoBoost = boost.GetbNoBoost();
	settings.RechargeDelay = boost.GetRechargeDelay();
	settings.RechargeRate = boost.GetRechargeRate();
	this->boostSettings = settings;
}

void KickoffPractice::resetBoostSettings()
{
	CarWrapper player = gameWrapper->GetLocalCar();
	if (!player) return;

	KickoffPractice::applyBoostSettings(player, this->boostSettings);
}

void KickoffPractice::applyBoostSettings(CarWrapper player, BoostSettings settings)
{
	auto boost = player.GetBoostComponent();
	if (!boost) return;

	boost.SetUnlimitedBoostRefCount(settings.UnlimitedBoostRefCount);
	boost.SetCurrentBoostAmount(settings.CurrentBoostAmount);
	boost.SetbNoBoost(settings.NoBoost);
	boost.SetRechargeDelay(settings.RechargeDelay);
	boost.SetRechargeRate(settings.RechargeRate);
}

void KickoffPractice::storeCarBodies()
{
	auto items = gameWrapper->GetItemsWrapper();

	if (!items)
	{
		this->nbCarBody = 1;
		this->carNames = new char* [1];
		this->carNames[0] = new char[14];
		strcpy(this->carNames[0], "No car found");

		return;
	}

	std::vector <std::string> itemLabels;
	for (auto item : items.GetAllProducts())
	{
		if (!item)
			continue;

		auto slot = item.GetSlot();
		// body slot has slot index 0
		if (!slot || slot.GetSlotIndex() != 0)
			continue;

		std::string label = item.GetLabel().ToString();
		itemLabels.push_back(label);

		this->carBodyIDs.push_back(item.GetID());
	}
	this->nbCarBody = static_cast<int>(itemLabels.size());
	this->carNames = new char* [this->nbCarBody];
	for (int i = 0; i < this->nbCarBody; i++)
	{
		this->carNames[i] = new char[128];
		if (itemLabels[i].size() < 128)
			strcpy(this->carNames[i], itemLabels[i].c_str());
		else
			strcpy(this->carNames[i], "Too long name :D");
	}
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

std::string KickoffPractice::getKickoffName(int kickoffId)
{
	switch (kickoffId)
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

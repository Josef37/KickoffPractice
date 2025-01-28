#include "pch.h"
#include "KickoffPractice.h"

BAKKESMOD_PLUGIN(KickoffPractice, "Kickoff Practice", plugin_version, PLUGINTYPE_FREEPLAY);

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
namespace fs = std::filesystem;

void KickoffPractice::onLoad()
{
	this->pluginEnabled = true;
	this->isInReplay = false;
	this->currentInputIndex = 0;
	this->isRecording = false;
	this->unlimitedBoostDefaultSetting = 1;
	this->rotationBot = Rotator(0, 0, 0);
	this->locationBot = Vector(0, 0, 0);
	this->tickCounter = 0;
	this->kickoffState = KickoffState::nothing;
	this->botJustSpawned = false;
	this->currentKickoffIndex = 0;
	this->botCarID = 0;
	this->selectedCarUI = 0;
	srand((int)time(0)); // initialize the random number generator seed

	_globalCvarManager = cvarManager;

	this->configPath = gameWrapper->GetDataFolderW() + L"/kickoffPractice";
	if (!fs::exists(configPath) || !fs::is_directory(configPath))
	{
		LOG("Creating directory");
		if (!fs::create_directory(configPath))
		{
			LOG("Can't create config directory in bakkesmod data folder");
		}
	}

	this->readConfigFile(configPath + L"/config.cfg");
	if (strlen(botKickoffFolder) == 0)
	{
		wcstombs(botKickoffFolder, (configPath + L"/bot").c_str(), sizeof(botKickoffFolder));
	}
	if (strlen(recordedKickoffFolder) == 0)
	{
		wcstombs(recordedKickoffFolder, (configPath + L"/recorded").c_str(), sizeof(recordedKickoffFolder));
	}
	if (!fs::exists(configPath + L"/bot") || !fs::is_directory(configPath + L"/bot"))
	{
		if (!fs::create_directory(configPath + L"/bot"))
			LOG("Can't create bot kickoff input directory in bakkesmod data folder");
	}
	if (!fs::exists(configPath + L"/recorded") || !fs::is_directory(configPath + L"/recorded"))
	{
		if (!fs::create_directory(configPath + L"/recorded"))
			LOG("Can't create recorded inputs directory in bakkesmod data folder");
	}

	this->loadInputFiles();

	this->storeCarBodies();

	cvarManager->registerNotifier("kickoff_train",
		[this](std::vector<std::string> args) {
			if (!pluginEnabled) return;
			// Use a timeout to start after other commands bound to the same button.
			gameWrapper->SetTimeout([this, args](GameWrapper* gameWrapper) {
				this->start(args, gameWrapper);
				}, 0.1f);

		},
		"Practice kickoff", PERMISSION_FREEPLAY);

	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.Car_TA.SetVehicleInput",
		[this](CarWrapper car, void* params, std::string eventName) {
			ControllerInput* input = static_cast<ControllerInput*>(params);
			this->onVehicleInput(car, input);
		});

	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.Car_TA.OnHitBall",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			if (this->kickoffState == KickoffState::started)
			{
				gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
					{
						if (kickoffState != KickoffState::started) return;
						this->reset();
					},
					this->timeAfterBackToNormal
				);
			}
		});

	gameWrapper->HookEvent("Function Engine.Actor.SpawnInstance",
		[this](std::string eventName) {
			if (!gameWrapper->IsInFreeplay()) return;
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
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
		});
	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.Ball_TA.OnHitGoal",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			this->isInReplay = true;
		});
	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.CarComponent_Boost_TA.SetUnlimitedBoost",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			recordBoost();
		});

	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function GameEvent_Soccar_TA.ReplayPlayback.EndState",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			this->isInReplay = false;
		});

	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function GameEvent_Soccar_TA.Countdown.BeginState",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			recordBoost();
			this->reset();
		});
}
void KickoffPractice::onUnload()
{
	for (int i = 0; i < nbCarBody; i++)
	{
		delete[] carNames[i];
	}
	delete[] carNames;
}


/// args[1] = kickoff location (1-5)
/// args[2] = is recording? (bool or 0/1)
void KickoffPractice::start(std::vector<std::string> args, GameWrapper* gameWrapper)
{
	if (!gameWrapper->IsInFreeplay()) return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server) return;
	if (this->isInReplay) return;
	this->recordBoost();
	this->reset();
	if (args.size() >= 3)
	{
		isRecording = (args[2] == "1" || args[2] == "true");
	}
	if (!isRecording && loadedKickoffIndices.size() == 0)
	{
		LOG("No inputs selected");
		return;
	}

	// Determine the current kickoff index.
	if (args.size() >= 2)
	{
		const int kickoffNumber = stoi(args[1]);
		if (kickoffNumber < 1 || kickoffNumber > 5) {
			LOG("The kickoff number argument should be between 1 and 5 (included).");
			return;
		}
		this->currentKickoffIndex = kickoffNumber - 1;
		auto it = std::find(loadedKickoffIndices.begin(), loadedKickoffIndices.end(), currentKickoffIndex);
		if (it == loadedKickoffIndices.end() && !isRecording)
		{
			LOG("No input found for this kickoff");
			return;
		}
	}
	else
	{
		if (loadedKickoffIndices.size() == 0)
		{
			LOG("No inputs selected");
			return;
		}
		int kickoffNumber = (rand() % loadedKickoffIndices.size());
		this->currentKickoffIndex = loadedKickoffIndices[kickoffNumber];
	}

	currentInputIndex = getRandomKickoffForId(currentKickoffIndex);
	if (currentInputIndex == -1 && (!isRecording))
	{
		LOG("Error, no input found for this kickoff");
		return;
	}

	LOG("isRecording: {}", isRecording);

	KickoffSide playerSide = isRecording ? KickoffSide::Orange : KickoffSide::Blue;
	Vector locationPlayer = getKickoffLocation(this->currentKickoffIndex, playerSide);
	Rotator rotationPlayer = Rotator(0, std::lroundf(getKickoffYaw(this->currentKickoffIndex, playerSide) * CONST_RadToUnrRot), 0);
	this->locationBot = getKickoffLocation(this->currentKickoffIndex, KickoffSide::Orange);
	this->rotationBot = Rotator(0, std::lroundf(getKickoffYaw(this->currentKickoffIndex, KickoffSide::Orange) * CONST_RadToUnrRot), 0);
	if (!isRecording)
	{
		server.SpawnBot(botCarID, "Kickoff Bot");
		this->botJustSpawned = true;
	}

	CarWrapper player = gameWrapper->GetLocalCar();
	// TODO: Duplication
	player.SetLocation(locationPlayer);
	player.SetRotation(rotationPlayer);
	player.Stop();

	BoostWrapper boost = player.GetBoostComponent();
	if (!boost) return;
	boost.SetUnlimitedBoostRefCount(0); // limit the player's boost (TODO: see if it's relevant for the bot)
	boost.SetCurrentBoostAmount(0.333f);

	// Reset boost pickups, because moving the player can cause picking up boost.
	gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
		{
			if (!gameWrapper->IsInFreeplay()) return;
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server) return;
			server.ResetPickups();
		},
		0.1f
	);

	BallWrapper ball = server.GetBall();
	if (!ball) return;
	ball.SetLocation(Vector(0, 0, ball.GetRadius()));
	ball.SetVelocity(Vector(0, 0, 0));
	ball.SetAngularVelocity(Vector(0, 0, 0), false);

	server.SendCountdownMessage(3, gameWrapper->GetPlayerController());
	gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
		{
			if (!gameWrapper->IsInFreeplay()) return;
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server) return;
			server.SendCountdownMessage(2, gameWrapper->GetPlayerController());
		}, 1);
	gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
		{
			if (!gameWrapper->IsInFreeplay()) return;
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server) return;
			server.SendCountdownMessage(1, gameWrapper->GetPlayerController());
		}, 2);
	gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
		{
			if (!gameWrapper->IsInFreeplay()) return;
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server) return;
			server.SendGoMessage(gameWrapper->GetPlayerController());
			this->kickoffState = KickoffState::started;
		}, 3);

	this->kickoffState = KickoffState::waitingToStart;

	if (isRecording)
	{
		LOG("Recording begins");
	}
}

void KickoffPractice::onVehicleInput(CarWrapper car, ControllerInput* input)
{
	if (!pluginEnabled) return;
	if (!gameWrapper->IsInFreeplay()) return;

	if (this->kickoffState == KickoffState::waitingToStart) {
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
			recordedInputs.push_back(*input);
		}
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
		return -1 * getKickoffLocation(kickoff, KickoffSide::Blue) + (2 * heightOffset);
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
		return getKickoffYaw(kickoff, KickoffSide::Blue) - CONST_PI_F;
	}
}

int KickoffPractice::getRandomKickoffForId(int kickoffId)
{
	std::vector<int> indices;
	for (int i = 0; i < loadedInputs.size(); i++)
	{
		if (states[i] - 1 == kickoffId)
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
	if (!gameWrapper->IsInFreeplay()) return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server) return;

	for (auto car : server.GetCars()) {
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

void KickoffPractice::storeCarBodies()
{
	auto items = gameWrapper->GetItemsWrapper();

	if (!items) {
		nbCarBody = 1;
		carNames = new char* [1];
		carNames[0] = new char[14];
		strcpy(carNames[0], "No car found");

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

		carBodyIDs.push_back(item.GetID());
	}
	nbCarBody = static_cast<int>(itemLabels.size());
	carNames = new char* [nbCarBody];
	for (int i = 0; i < nbCarBody; i++)
	{
		carNames[i] = new char[128];
		if (itemLabels[i].size() < 128)
			strcpy(carNames[i], itemLabels[i].c_str());
		else
			strcpy(carNames[i], "Too long name :D");
	}
}

void KickoffPractice::reset()
{
	this->removeBots();

	if (isRecording)
	{
		const size_t numberOfInputs = recordedInputs.size();
		LOG("Recording ends. Ticks recorded : {}", numberOfInputs);

		auto time = std::time(nullptr);
		std::ostringstream oss;
		oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H-%M-%S");
		auto timestamp = oss.str();

		auto name = this->getKickoffName(this->currentKickoffIndex);
		auto filename = "\\" + name + " " + timestamp + ".kinputs";
		std::ofstream inputFile(recordedKickoffFolder + filename);
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
			const ControllerInput input = recordedInputs[i];

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
	this->resetBoost();
	this->isInReplay = false;
	this->isRecording = false;
}

void KickoffPractice::recordBoost()
{
	if (!gameWrapper->IsInFreeplay()) return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server) return;
	if (this->isInReplay) return;
	auto player = gameWrapper->GetLocalCar();
	if (!player) return;
	auto boost = player.GetBoostComponent();
	if (!boost) return;
	this->unlimitedBoostDefaultSetting = boost.GetUnlimitedBoostRefCount(); // bugged
}

void KickoffPractice::updateLoadedKickoffIndices()
{
	loadedKickoffIndices = std::vector<int>();
	for (int i = 0; i < states.size(); i++)
	{
		if (states[i] == 0) continue;
		int index = states[i] - 1;
		auto it = std::find(loadedKickoffIndices.begin(), loadedKickoffIndices.end(), index);
		if (it == loadedKickoffIndices.end()) // if we haven't found it
		{
			loadedKickoffIndices.push_back(index);
		}
	}
}


void KickoffPractice::loadInputFiles()
{
	loadedInputs = std::vector<RecordedKickoff>();
	states = std::vector<int>();

	try
	{
		for (const auto& entry : fs::directory_iterator(botKickoffFolder))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".kinputs")
			{
				loadedInputs.push_back(readKickoffFile(entry.path().string(), entry.path().filename().string()));
				states.push_back(0);
			}
		}
	}
	catch (std::filesystem::filesystem_error const& ex)
	{
		LOG("ERROR : {}", ex.code().message());
	}
	this->readConfigFile(configPath + L"/config.cfg");
	updateLoadedKickoffIndices();
}

void KickoffPractice::resetBoost()
{
	if (!gameWrapper->IsInFreeplay())return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server)return;

	CarWrapper player = gameWrapper->GetLocalCar();
	if (!player)return;
	BoostWrapper boost = player.GetBoostComponent();
	if (!boost)return;
	boost.SetUnlimitedBoostRefCount(this->unlimitedBoostDefaultSetting);//TODO: it always gives unlimited boost so it's buggy
	if (unlimitedBoostDefaultSetting > 0)
	{
		boost.SetBoostAmount(1.0);
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
		return "Back Right Corner";
	case 3:
		return "Back Left Corner";
	case 4:
		return "Far Back";
	default:
		return "Unknown";
	}
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

			if (i == 1) {
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
				for (int j = 0; j < loadedInputs.size(); j++)
				{
					if (loadedInputs[j].name == name)
					{
						states[j] = state;
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

	if (botFolder.size() < 128 && botFolder.size() > 0)
	{
		strcpy(this->botKickoffFolder, botFolder.c_str());
	}
	else if (botFolder.size() < 128)
	{
		wcstombs(botKickoffFolder, (configPath + L"/bot").c_str(), sizeof(botKickoffFolder));
	}
	if (recordFolder.size() < 128 && recordFolder.size() > 0)
	{
		strcpy(this->recordedKickoffFolder, recordFolder.c_str());
	}
	else if (recordFolder.size() < 128)
	{
		wcstombs(recordedKickoffFolder, (configPath + L"/recorded").c_str(), sizeof(recordedKickoffFolder));
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
	inputFile << botKickoffFolder << "\n";
	inputFile << recordedKickoffFolder << "\n";

	for (int i = 0; i < states.size(); i++)
	{
		inputFile << states[i] << "," << this->loadedInputs[i].name << "\n";
	}
	inputFile.close();
}

void KickoffPractice::RenderSettings()
{
	ImGui::Checkbox("Enable plugin", &pluginEnabled);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Enable or disable the plugin");
	}
	ImGui::NewLine();
	ImGui::SliderFloat("Time before back to normal", &this->timeAfterBackToNormal, 0.0f, 3.0f, "%.3f seconds");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How long you stay in \"kickoff mode\" after someone hit the ball. This also affects how long the recording lasts after hitting the ball.");
	}
	ImGui::NewLine();
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Should a bot be spawned when recording inputs");
	}
	ImGui::Separator();

#pragma region browseRecord

	ImGui::TextUnformatted("Select a folder to record kickoffs in");
	ImGui::SameLine();
	if (ImGui::Button(".."))
	{
		ImGui::OpenPopup("browseRecord");
	}
	ImGui::SetNextWindowSize(ImVec2(500, 500));

	if (ImGui::BeginPopup("browseRecord", ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar))
	{
		std::string selPath = botMenu.main();
		if (selPath != "")
		{
			if (selPath.size() < 128)
			{
				//LOG(inputPath);
				strcpy(recordedKickoffFolder, selPath.c_str());
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PushID(42); // otherwise ImGui confuses textboxes without labels
	ImGui::InputText("", recordedKickoffFolder, IM_ARRAYSIZE(recordedKickoffFolder));
	ImGui::PopID();

#pragma endregion
	for (int i = 0; i < 5; i++)
	{
		if (ImGui::Button(("Record " + getKickoffName(i)).c_str()))
		{
			gameWrapper->Execute([this, i](GameWrapper* gw) {
				cvarManager->executeCommand("kickoff_train " + std::to_string(i + 1) + " true;closemenu settings");
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(("Record input for the " + getKickoffName(i) + " kickoff").c_str());
		}
		if (i != 4)ImGui::SameLine();
	}

	ImGui::Separator();

#pragma region browseKickoff

	ImGui::TextUnformatted("Select a folder to browse for kickoffs");
	ImGui::SameLine();
	if (ImGui::Button("..."))
	{
		ImGui::OpenPopup("browseKickoff");
	}

	ImGui::SetNextWindowSize(ImVec2(500, 500));


	if (ImGui::BeginPopup("browseKickoff", ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar))
	{
		std::string inputPath = recordMenu.main();
		if (inputPath != "")
		{
			if (inputPath.size() < 128)
			{
				strcpy(botKickoffFolder, inputPath.c_str());
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PushID(69); // otherwise ImGui confuses textboxes without labels
	ImGui::InputText("", botKickoffFolder, IM_ARRAYSIZE(botKickoffFolder));
	ImGui::PopID();


	if (ImGui::Button("Validate"))
	{
		loadInputFiles();
		LOG("{}", loadedInputs.size());
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Validate changes");
	}

	if (loadedKickoffIndices.size() == 0)
	{
		ImGui::TextColored(ImVec4(255, 0, 0, 255), "No kickoff selected !");
	}

#pragma endregion

	ImGui::BeginGroup();
	ImGui::TextUnformatted("");

	const ImGuiWindowFlags child_flags = 0;
	const ImGuiID child_id = ImGui::GetID((void*)(intptr_t)0);
	const bool child_is_visible = ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true, child_flags);

	size_t count = loadedInputs.size();
	const char* items[] = { "Unused", "Right Corner", "Left Corner", "Back Right", "Back Left", "Far Back Center" };
	bool isChanged = false;
	if (child_is_visible)
	{
		ImGui::Indent(5);
		for (int i = 0; i < count; i++)
		{
			ImGui::PushID(i);
			isChanged = isChanged || ImGui::Combo(loadedInputs[i].name.c_str(), &states[i], items, IM_ARRAYSIZE(items));
			ImGui::PopID();
		}
	}

	float scroll_y = ImGui::GetScrollY();
	float scroll_max_y = ImGui::GetScrollMaxY();
	ImGui::EndChild();
	ImGui::EndGroup();

	if (isChanged)
	{
		updateLoadedKickoffIndices();
		writeConfigFile(configPath + L"/config.cfg");
	}

	if (nbCarBody == -1)
		return;
	if (ImGui::Combo("Bot car body", &selectedCarUI, carNames, nbCarBody))
	{
		this->botCarID = carBodyIDs[selectedCarUI];
	}
}


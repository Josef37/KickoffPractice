#include "pch.h"
#include "kickoffPractice.h"
#include "pathInput.h"
#define _USE_MATH_DEFINES

#include <math.h>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <stdlib.h>

BAKKESMOD_PLUGIN(kickoffPractice, "Kickoff Practice", plugin_version, PERMISSION_ALL)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
namespace fs = std::filesystem;

void kickoffPractice::onLoad()
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

	auto items = gameWrapper->GetItemsWrapper();
	if (items)
	{
		std::vector <std::string> itemLabels;
		for (auto item : items.GetAllProducts())
		{
			if (!item)
			{
				continue;
			}
			auto slot = item.GetSlot();
			if (!slot || slot.GetSlotIndex() != 0) // body slot has slotIndex 0
			{
				continue;
			}
			std::string label = item.GetLabel().ToString();
			itemLabels.push_back(label);
		}
		nbCarBody = itemLabels.size();
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
	else
	{
		nbCarBody = 1;
		carNames = new char* [1];
		carNames[0] = new char[14];
		strcpy(carNames[0], "No car found");
	}
	storeBodyIndex();

	cvarManager->registerNotifier("kickoff_train", [this](std::vector<std::string> args) {
		this->start(args);
		}, "Practice kickoff", PERMISSION_FREEPLAY);
	gameWrapper->HookEvent("Function TAGame.EngineShare_TA.EventPostPhysicsStep", [this](std::string eventName) {
		this->tick(eventName);
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
			if (!this->botJustSpawned) return;
			ArrayWrapper<CarWrapper> tutures = server.GetCars();

			for (int i = 0; i < tutures.Count(); i++)
			{
				if (tutures.Get(i).GetPRI().GetbBot())
				{
					tutures.Get(i).SetCarRotation(this->rotationBot);
					tutures.Get(i).SetLocation(this->locationBot);
					tutures.Get(i).SetVelocity(Vector(0, 0, 0));
					AIControllerWrapper controller = tutures.Get(i).GetAIController();
					if (!controller)continue;
					controller.DoNothing();
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
void kickoffPractice::onUnload()
{
	for (int i = 0; i < nbCarBody; i++)
	{
		delete[] carNames[i];
	}
	delete[] carNames;
}

std::string kickoffPractice::GetPluginName()
{
	return "Kickoff Practice";
}

void kickoffPractice::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void kickoffPractice::start(std::vector<std::string> args)
{
	if (!pluginEnabled) return;
	gameWrapper->SetTimeout([this, args](GameWrapper* gameWrapper)
		{
			if (!gameWrapper->IsInFreeplay())return;
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server)return;
			if (this->isInReplay)return;
			this->recordBoost();
			this->reset();
			if (args.size() >= 3)
			{
				isRecording = (args[2] == "1" || args[2] == "true");
			}
			if (!isRecording && kickoffDispo.size() == 0)
			{
				LOG("No inputs selected");
				return;
			}
			if (args.size() >= 2)
			{
				const int kickoffNb = stoi(args[1]);
				if (kickoffNb >= 1 && kickoffNb <= 5)
				{
					this->currentKickoffIndex = kickoffNb - 1;
					auto it = std::find(kickoffDispo.begin(), kickoffDispo.end(), currentKickoffIndex);
					if (it == kickoffDispo.end() && !isRecording)
					{
						LOG("No input found for this kickoff");
						return;
					}
				}
				else
				{
					LOG("The kickoff number argument should be between 1 and 5 (included).");
					return;
				}

			}
			else
			{
				if (kickoffDispo.size() == 0)
				{
					LOG("No inputs selected");
					return;
				}
				int iKickoffNb = (rand() % kickoffDispo.size());
				currentKickoffIndex = kickoffDispo[iKickoffNb];
			}

			currentInputIndex = getRandomKickoffForId(currentKickoffIndex);
			if (currentInputIndex == -1 && (!isRecording))
			{
				LOG("Error, no input found for this kickoff");
				return;
			}

			LOG("isRecording: {}", isRecording);

			Vector locationPlayer = getKickoffLocation(this->currentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE : KICKOFF_BLUE_SIDE);
			Rotator rotationPlayer = Rotator(0, getKickoffYaw(this->currentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE : KICKOFF_BLUE_SIDE) * CONST_RadToUnrRot, 0);
			this->locationBot = getKickoffLocation(this->currentKickoffIndex, KICKOFF_ORANGE_SIDE);
			this->rotationBot = Rotator(0, getKickoffYaw(this->currentKickoffIndex, KICKOFF_ORANGE_SIDE) * CONST_RadToUnrRot, 0);
			if (!isRecording)
			{
				server.SpawnBot(botCarID, "Kickoff Bot"); // spawn a bot knowing that it is the only one

				this->botJustSpawned = true;
			}
			ArrayWrapper<CarWrapper> cars = server.GetCars();

			int nbCars = cars.Count();
			if (nbCars != 2 && !(isRecording))
			{
				LOG("ERROR ! Wrong amount of cars on the field");
				return;
			}
			const bool botIsFirst = cars.Get(0).GetPRI().GetbBot();
			CarWrapper player = botIsFirst ? cars.Get(1) : cars.Get(0);
			CarWrapper bot = botIsFirst ? cars.Get(0) : cars.Get(1);

			player.SetLocation(locationPlayer);
			player.SetRotation(rotationPlayer);
			player.SetVelocity(Vector(0, 0, 0));

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
			if (!ball)return;
			ball.SetLocation(Vector(0, 0, 0));
			ball.SetAngularVelocity(Vector(0, 0, 0), false);
			ball.SetVelocity(Vector(0, 0, 0));

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

		}, 0.1);
}

void kickoffPractice::tick(std::string eventName)
{
	if (!pluginEnabled) return;
	if (!gameWrapper->IsInFreeplay()) return;
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	if (this->kickoffState == KickoffState::nothing) return;

	ArrayWrapper<CarWrapper> cars = server.GetCars();
	CarWrapper* player = nullptr;
	CarWrapper* bot = nullptr;
	int nbCars = cars.Count();
	if (nbCars == 1)
	{
		player = new CarWrapper(cars.Get(0));
	}
	else if (nbCars == 2)
	{
		const bool botIsFirst = cars.Get(0).GetPRI().GetbBot();
		player = new CarWrapper(botIsFirst ? cars.Get(1) : cars.Get(0));
		bot = new CarWrapper(botIsFirst ? cars.Get(0) : cars.Get(1));
	}
	else return;

	if (this->kickoffState == KickoffState::started)
	{
		ControllerInput idle;
		if (nbCars == 2)
		{
			if (this->tickCounter >= this->loadedInputs[this->currentInputIndex].inputs.size())
			{
				bot->SetInput(idle);
			}
			else
			{
				CarState state = this->loadedInputs[this->currentInputIndex].inputs[++(this->tickCounter)];
				bot->SetLocation(state.location);
				bot->SetVelocity(state.velocity);
				bot->SetRotation(state.rotation);
				bot->SetAngularVelocity(state.angularVelocity, false);
				bot->SetbOverrideHandbrakeOn(state.handbrakeOn);
				bot->GetBoostComponent().SetBoostAmount(state.boostAmount);
				bot->ForceBoost(state.isBoosting);
			}
		}
		if (isRecording)
		{
			CarState CarTemp;
			CarTemp.location = player->GetLocation();
			CarTemp.velocity = player->GetVelocity();
			CarTemp.rotation = player->GetRotation();
			CarTemp.angularVelocity = player->GetAngularVelocity();
			CarTemp.steer = player->GetReplicatedSteer();
			CarTemp.throttle = player->GetReplicatedThrottle();
			CarTemp.handbrakeOn = player->GetbReplicatedHandbrake();
			CarTemp.boostAmount = player->GetBoostComponent().IsNull() ? 0 : player->GetBoostComponent().GetCurrentBoostAmount();
			CarTemp.isBoosting = player->IsBoostCheap();
			recordedInputs.push_back(CarTemp);
		}
	}
	else
	{
		player->SetLocation(getKickoffLocation(this->currentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE : KICKOFF_BLUE_SIDE));
		player->SetRotation(Rotator(0, getKickoffYaw(this->currentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE : KICKOFF_BLUE_SIDE) * CONST_RadToUnrRot, 0));
		player->SetVelocity(Vector(0, 0, 0));
		BoostWrapper boost = player->GetBoostComponent();
		if (!boost)return;
		boost.SetBoostAmount(0.333f);
	}
	delete bot;
	delete player;
}


Vector kickoffPractice::getKickoffLocation(int kickoff, bool side)
{
	if (side == KICKOFF_BLUE_SIDE)
	{
		if (kickoff == KICKOFF_RIGHT_CORNER)
			return Vector(-2048, -2560, 20);
		if (kickoff == KICKOFF_LEFT_CORNER)
			return Vector(2048, -2560, 20);
		if (kickoff == KICKOFF_BACK_RIGHT)
			return Vector(-256, -3840, 20);
		if (kickoff == KICKOFF_BACK_LEFT)
			return Vector(256.0, -3840, 20);
		if (kickoff == KICKOFF_FAR_BACK_CENTER)
			return Vector(0.0, -4608, 20);
	}
	else
	{
		if (kickoff == KICKOFF_RIGHT_CORNER)
			return Vector(2048, 2560, 20);
		if (kickoff == KICKOFF_LEFT_CORNER)
			return Vector(-2048, 2560, 20);
		if (kickoff == KICKOFF_BACK_RIGHT)
			return Vector(256.0, 3840, 20);
		if (kickoff == KICKOFF_BACK_LEFT)
			return Vector(-256.0, 3840, 20);
		if (kickoff == KICKOFF_FAR_BACK_CENTER)
			return Vector(0.0, 4608, 20);
	}
}

float kickoffPractice::getKickoffYaw(int kickoff, bool side)
{
	if (side == KICKOFF_BLUE_SIDE)
	{
		if (kickoff == KICKOFF_RIGHT_CORNER)
			return 0.25 * M_PI;
		if (kickoff == KICKOFF_LEFT_CORNER)
			return 0.75 * M_PI;
		if (kickoff == KICKOFF_BACK_RIGHT)
			return 0.5 * M_PI;
		if (kickoff == KICKOFF_BACK_LEFT)
			return 0.5 * M_PI;
		if (kickoff == KICKOFF_FAR_BACK_CENTER)
			return 0.5 * M_PI;
	}
	else
	{
		if (kickoff == KICKOFF_RIGHT_CORNER)
			return -0.75 * M_PI;
		if (kickoff == KICKOFF_LEFT_CORNER)
			return -0.25 * M_PI;
		if (kickoff == KICKOFF_BACK_RIGHT)
			return -0.5 * M_PI;
		if (kickoff == KICKOFF_BACK_LEFT)
			return -0.5 * M_PI;
		if (kickoff == KICKOFF_FAR_BACK_CENTER)
			return -0.5 * M_PI;
	}
}

int kickoffPractice::getRandomKickoffForId(int kickoffId)
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

void kickoffPractice::removeBots()
{
	if (!gameWrapper->IsInFreeplay())return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server)return;
	ArrayWrapper<CarWrapper> cars = server.GetCars();

	for (int i = 0; i < cars.Count(); i++)
	{
		if (cars.Get(i).GetPRI().GetbBot())
		{
			cars.Get(i).GetPRI().Unregister();
			cars.Get(i).Destroy();
		}
	}
}

void kickoffPractice::storeBodyIndex()
{
	auto items = gameWrapper->GetItemsWrapper();
	if (!items) return;
	for (auto item : items.GetAllProducts())
	{
		if (!item)
		{
			continue;
		}
		auto slot = item.GetSlot();
		if (!slot || slot.GetSlotIndex() != 0) // body slot has slotIndex 0
		{
			continue;
		}
		carBodyIDs.push_back(item.GetID());

	}
}

void kickoffPractice::reset()
{
	this->removeBots();

	if (isRecording)
	{
		cvarManager->log("Recording ends");
		const int nbInputs = recordedInputs.size();
		LOG("Ticks recorded : {}", nbInputs);
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);

		std::ostringstream oss;
		oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
		auto str = oss.str();
		str = "\\recordedInput" + str + ".kstates";
		std::ofstream inputFile(recordedKickoffFolder + str);
		if (!inputFile.is_open())
		{
			LOG("ERROR : can't create recording file");
			return;
		}
		for (int i = 0; i < nbInputs; i++)
		{
			const CarState mvtPlayer = recordedInputs[i];
			inputFile << mvtPlayer.location.X << ", " << mvtPlayer.location.Y << ", " << mvtPlayer.location.Z
				<< ", " << mvtPlayer.velocity.X << ", " << mvtPlayer.velocity.Y << ", " << mvtPlayer.velocity.Z
				<< ", " << mvtPlayer.rotation.Pitch << ", " << mvtPlayer.rotation.Roll << ", " << mvtPlayer.rotation.Yaw
				<< ", " << mvtPlayer.angularVelocity.X << ", " << mvtPlayer.angularVelocity.Y << ", " << mvtPlayer.angularVelocity.Z
				<< ", " << mvtPlayer.steer
				<< ", " << mvtPlayer.throttle
				<< ", " << mvtPlayer.handbrakeOn
				<< ", " << mvtPlayer.boostAmount
				<< ", " << mvtPlayer.isBoosting << "\n";
		}
		inputFile.close();
		this->recordedInputs = std::vector<CarState>();
	}

	this->kickoffState = KickoffState::nothing;
	this->tickCounter = 0;
	this->currentKickoffIndex = 0;
	this->resetBoost();
	this->isInReplay = false;
	this->isRecording = false;
}

void kickoffPractice::recordBoost()
{
	if (!gameWrapper->IsInFreeplay()) return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server) return;
	if (this->isInReplay) return;
	auto player = gameWrapper->GetLocalCar();
	if (!player) return;
	auto boost = player.GetBoostComponent();
	if (!boost) return;
	this->unlimitedBoostDefaultSetting = boost.GetUnlimitedBoostRefCount();// /�\bugged
}

void kickoffPractice::checkForKickoffDispo()
{
	kickoffDispo = std::vector<int>();
	for (int i = 0; i < states.size(); i++)
	{
		if (states[i] == 0)continue;
		int vraiIndex = states[i] - 1;
		auto it = std::find(kickoffDispo.begin(), kickoffDispo.end(), vraiIndex);
		if (it == kickoffDispo.end()) // if we haven't found it
		{
			kickoffDispo.push_back(vraiIndex);
		}
	}
}


void kickoffPractice::loadInputFiles()
{
	loadedInputs = std::vector<RecordedKickoff>();
	states = std::vector<int>();

	try
	{
		for (const auto& entry : fs::directory_iterator(botKickoffFolder))
		{
			//LOG(entry.path().filename().string());
			if (entry.is_regular_file() && entry.path().extension() == ".kstates")
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
	checkForKickoffDispo();
}

void kickoffPractice::resetBoost()
{
	if (!gameWrapper->IsInFreeplay())return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server)return;

	CarWrapper player = gameWrapper->GetLocalCar();
	if (!player)return;
	BoostWrapper boost = player.GetBoostComponent();
	if (!boost)return;
	//LOG("set unlimited boost settings as : {} ", unlimitedBoostDefaultSetting);
	boost.SetUnlimitedBoostRefCount(this->unlimitedBoostDefaultSetting);//TODO: �a remet toujours unlimited boost donc c bugg�
	if (unlimitedBoostDefaultSetting > 0)
	{
		boost.SetBoostAmount(1.0);
	}
}

std::string kickoffPractice::getKickoffName(int kickoffId)
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


RecordedKickoff kickoffPractice::readKickoffFile(std::string fileName, std::string kickoffName)
{
	std::vector<std::string> row;
	std::string line, word;
	std::vector<CarState> currentInputs;

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
			// store in the appropriate structure
			CarState state;
			if (row.size() != 17)
			{
				LOG("Error on line {} : size of {} instead of 17", i, row.size());
				continue;
			}
			try
			{
				state.location.X = std::stof(row[0]);
				state.location.Y = std::stof(row[1]);
				state.location.Z = std::stof(row[2]);

				state.velocity.X = std::stof(row[3]);
				state.velocity.Y = std::stof(row[4]);
				state.velocity.Z = std::stof(row[5]);

				state.rotation.Pitch = std::stoi(row[6]);
				state.rotation.Roll = std::stoi(row[7]);
				state.rotation.Yaw = std::stoi(row[8]);

				state.angularVelocity.X = std::stof(row[9]);
				state.angularVelocity.Y = std::stof(row[10]);
				state.angularVelocity.Z = std::stof(row[11]);

				state.steer = std::stof(row[12]);
				state.throttle = std::stof(row[13]);
				state.handbrakeOn = std::stoul(row[14]);
				state.boostAmount = std::stof(row[15]);
				state.isBoosting = (row[16] == "true");

				currentInputs.push_back(state);
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
		LOG("Can't open {}", fileName);

	LOG("{} : {} lines loaded", fileName, currentInputs.size());
	RecordedKickoff a;
	a.name = kickoffName;
	a.inputs = currentInputs;
	return a;
}

void kickoffPractice::readConfigFile(std::wstring fileName)
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
				if (state < 0 || state > 5)continue;
				for (int j = 0; j < loadedInputs.size(); j++)
				{
					if (loadedInputs[j].name == name)
					{
						//LOG("Found state for {}", name);
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

void kickoffPractice::writeConfigFile(std::wstring fileName)
{
	std::ofstream inputFile(fileName);
	//LOG(fs::absolute(fileName).c_str());
	if (!inputFile.is_open())
	{
		LOG("ERROR : can't create config file");
		return;
	}
	inputFile << botKickoffFolder << "\n";
	inputFile << botKickoffFolder << "\n";

	for (int i = 0; i < states.size(); i++)
	{
		inputFile << states[i] << "," << this->loadedInputs[i].name << "\n";
	}
	inputFile.close();
}

void kickoffPractice::RenderSettings()
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
		ImGui::SetTooltip("How long you stay in \"kickoff mode\" after someone hit the ball");
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
				//LOG(inputPath);
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

	int nbPossible = kickoffDispo.size();
	if (nbPossible == 0)
	{
		ImGui::TextColored(ImVec4(255, 0, 0, 255), "No kickoff selected !");
	}

#pragma endregion

	ImGui::BeginGroup();
	ImGui::TextUnformatted("");

	const ImGuiWindowFlags child_flags = 0;
	const ImGuiID child_id = ImGui::GetID((void*)(intptr_t)0);
	const bool child_is_visible = ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true, child_flags);

	int count = loadedInputs.size();
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
		//LOG("Changement ou bouton");
		checkForKickoffDispo();
		writeConfigFile(configPath + L"/config.cfg");
	}

	if (nbCarBody == -1)
		return;
	if (ImGui::Combo("Bot car body", &selectedCarUI, carNames, nbCarBody))
	{
		this->botCarID = carBodyIDs[selectedCarUI];
	}
}


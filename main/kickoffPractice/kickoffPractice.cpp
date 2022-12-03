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
#include <cstdlib>
#include <stdlib.h>

BAKKESMOD_PLUGIN(kickoffPractice, "A plugin to train kickoff by facing a bot doing prerecorded inputs in freeplay", plugin_version, PERMISSION_ALL)

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
	this->tickCompteur = 0;
	this->kickoffState = KickoffStates::nothing;
	this->botJustSpawned = false;
	this->currrentKickoffIndex = 0;
	this->botCarID = 0;
	this->selectedCarUI = 0;
	srand((int)time(0));//initialize the random c thingy


	//readKickoffFile("C:\\Users\\Antonin\\Desktop\\recordedInput.txt");
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

	this->readConfigFile(configPath +L"/config.cfg");
	if (strlen(botKickoffFolder) == 0)
	{
		wcstombs(botKickoffFolder,(configPath + L"/bot").c_str(),sizeof(botKickoffFolder));
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
	std::vector < std::string> temp;
	if (items)
	{
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
			std::string nom = item.GetLabel().ToString();
			temp.push_back(nom);
		}
		nbCarBody = temp.size();
		carNames = new char* [nbCarBody];
		for (int i = 0;i < nbCarBody;i++)
		{
			carNames[i] = new char[128];
			if (temp[i].size() < 128)
				strcpy(carNames[i], temp[i].c_str());
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

	//for (int i = 0; i < carNames.size();i++)
		//LOG(carNames[i]);

	cvarManager->registerNotifier("kickoff_train", [this](std::vector<std::string> args) {
		this->start(args);
		}, "Practice kickoff", PERMISSION_FREEPLAY);
	gameWrapper->HookEvent("Function TAGame.EngineShare_TA.EventPostPhysicsStep", bind(&kickoffPractice::tick, this, std::placeholders::_1));

	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.Car_TA.OnHitBall",
		[this](CarWrapper caller, void* params, std::string eventname) 
		{
			//si on est en séquence de kickoff quand on a touché la balle
			if (this->kickoffState == KickoffStates::started)
			{
				gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
				{
					if (kickoffState != KickoffStates::started)return;
					//LOG("On reset parce qu'il a touché la balle :)");
					this->reset();
				}, this->timeAfterBackToNormal);
			}
		});

	gameWrapper->HookEvent("Function Engine.Actor.SpawnInstance",
		[this](std::string eventName) {
			if (!gameWrapper->IsInFreeplay())return;
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server)return;
			if (!this->botJustSpawned)return;
			ArrayWrapper<CarWrapper> tutures = server.GetCars();
			//cvarManager->log("nb voiture : " + std::to_std::string(tutures.Count()));
			for (int i = 0; i < tutures.Count();i++)
			{
				//cvarManager->log(!tutures.Get(i).GetPRI().GetbBot() ? "bot" : "humain");
				if (tutures.Get(i).GetPRI().GetbBot())
				{
					tutures.Get(i).SetCarRotation(this->rotationBot);
					tutures.Get(i).SetLocation(this->locationBot);			//même tarif pour le bot
					tutures.Get(i).SetVelocity(Vector(0, 0, 0));
					AIControllerWrapper cerveau = tutures.Get(i).GetAIController();
					if (!cerveau)continue;
					cerveau.DoNothing();
				}
			}
			this->botJustSpawned = false;
		});
	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.Ball_TA.OnHitGoal",
		[this](CarWrapper caller, void* params, std::string eventname)
		{
			//LOG("in replay");
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
			//LOG("Not in replay");
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
	for (int i = 0; i < nbCarBody;i++)
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
	if (!pluginEnabled)return;
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
					this->currrentKickoffIndex = kickoffNb - 1;
					auto it = std::find(kickoffDispo.begin(), kickoffDispo.end(), currrentKickoffIndex);
					if (it == kickoffDispo.end() && !isRecording)//si on l'a pas trouvé
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
				currrentKickoffIndex = kickoffDispo[iKickoffNb];
			}

			currentInputIndex = getRandomKickoffForId(currrentKickoffIndex);
			if (currentInputIndex == -1 && (!isRecording))
			{
				LOG("Error, no input found for this kickoff");
				return;
			}
			//cvarManager->executeCommand("clear");

			LOG("{}", isRecording);

			//cvarManager->log("Kickoff numero : " + std::to_string(currrentKickoffIndex));
			Vector locationJoueur = getKickoffLocation(this->currrentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE : KICKOFF_BLUE_SIDE);
			Rotator rotationJoueur = Rotator(0, getKickoffYaw(this->currrentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE : KICKOFF_BLUE_SIDE) * CONST_RadToUnrRot, 0);
			this->locationBot = getKickoffLocation(this->currrentKickoffIndex, KICKOFF_ORANGE_SIDE);
			this->rotationBot = Rotator(0, getKickoffYaw(this->currrentKickoffIndex, KICKOFF_ORANGE_SIDE) * CONST_RadToUnrRot, 0);
			if (!isRecording)
			{
				server.SpawnBot(botCarID, "Kickoff Bot");//on spawn un bot en sachant qu'il est le seul

				//cvarManager->log("On spawn un bot");

				this->botJustSpawned = true;
			}
			ArrayWrapper<CarWrapper> tutures = server.GetCars();

			int nbTutures = tutures.Count();
			if (nbTutures != 2 && !(isRecording))//si on a pas spawn la voiture ou qu'on a pas pu
			{
				LOG("ERROR ! Wrong amount of car on the field");
				return;
			}
			const bool botIsFirst = tutures.Get(0).GetPRI().GetbBot();
			CarWrapper joueur = botIsFirst ? tutures.Get(1) : tutures.Get(0);
			CarWrapper bot = botIsFirst ? tutures.Get(0) : tutures.Get(1);

			joueur.SetLocation(locationJoueur);
			joueur.SetRotation(rotationJoueur);
			joueur.SetVelocity(Vector(0, 0, 0));

			BoostWrapper boost = joueur.GetBoostComponent();
			if (!boost)return;
			//LOG("Unlimited boost setting : {}, consumption rate : {}", boost.GetUnlimitedBoostRefCount(),boost.GetBoostConsumptionRate());

			boost.SetUnlimitedBoostRefCount(0);			//on limite le boost du joueur (TODO: voir si c'ests utile pour le bot)
			boost.SetCurrentBoostAmount(0.3333f);

			BallWrapper ball = server.GetBall();
			if (!ball)return;
			ball.SetLocation(Vector(0, 0, 0));
			ball.SetAngularVelocity(Vector(0, 0, 0), false);
			ball.SetVelocity(Vector(0, 0, 0));

			server.SendCountdownMessage(3, gameWrapper->GetPlayerController());
			gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
				{
					if (!gameWrapper->IsInFreeplay())return;
					ServerWrapper server = gameWrapper->GetGameEventAsServer();
					if (!server)return;
					server.SendCountdownMessage(2, gameWrapper->GetPlayerController());
				}, 1);
			gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
				{
					if (!gameWrapper->IsInFreeplay())return;
					ServerWrapper server = gameWrapper->GetGameEventAsServer();
					if (!server)return;
					server.SendCountdownMessage(1, gameWrapper->GetPlayerController());
				}, 2);
			gameWrapper->SetTimeout([this](GameWrapper* gameWrapper)
				{
					if (!gameWrapper->IsInFreeplay())return;
					ServerWrapper server = gameWrapper->GetGameEventAsServer();
					if (!server)return;
					server.SendGoMessage(gameWrapper->GetPlayerController());
					this->kickoffState = KickoffStates::started;
				}, 3);
			this->kickoffState = KickoffStates::waitingToStart;

			if (isRecording)
			{
				LOG("Recording begins");
			}

		}, 0.1);
}

void kickoffPractice::tick(std::string eventName)
{
	if (!pluginEnabled)return;
	if (!gameWrapper->IsInFreeplay())return;
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server)return;
	if (this->kickoffState == KickoffStates::nothing)
	{
		return;
	}

	ArrayWrapper<CarWrapper> tutures = server.GetCars();
	CarWrapper *joueur = nullptr;
	CarWrapper* bot = nullptr;
	int nbTutures = tutures.Count();
	if (nbTutures == 1)
	{
		joueur = new CarWrapper(tutures.Get(0));
	}
	else if (nbTutures == 2)
	{

		const bool botIsFirst = tutures.Get(0).GetPRI().GetbBot();
		joueur = new CarWrapper(botIsFirst ? tutures.Get(1) : tutures.Get(0));
		bot = new CarWrapper(botIsFirst ? tutures.Get(0) : tutures.Get(1));
	}
	else return;

	if (this->kickoffState == KickoffStates::started )
	{
		ControllerInput iddle;
		if (nbTutures == 2)
		{
			if (this->tickCompteur >= this->loadedInputs[this->currentInputIndex].inputs.size())
			{
				bot->SetInput(iddle);
			}
			else
			{
				carState state = this->loadedInputs[this->currentInputIndex].inputs[++(this->tickCompteur)];
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
			carState CarTemp;
			CarTemp.location = joueur->GetLocation();
			CarTemp.velocity = joueur->GetVelocity();
			CarTemp.rotation = joueur->GetRotation();
			CarTemp.angularVelocity= joueur->GetAngularVelocity();
			CarTemp.steer = joueur->GetReplicatedSteer();
			CarTemp.throttle = joueur->GetReplicatedThrottle();
			CarTemp.handbrakeOn = joueur->GetbReplicatedHandbrake();
			CarTemp.boostAmount = joueur->GetBoostComponent().IsNull() ? 0 : joueur->GetBoostComponent().GetCurrentBoostAmount();
			CarTemp.isBoosting = joueur->IsBoostCheap();
			recordedInputs.push_back(CarTemp);
		}
	}
	else
	{
		joueur->SetLocation(getKickoffLocation(this->currrentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE: KICKOFF_BLUE_SIDE));
		joueur->SetRotation(Rotator(0, getKickoffYaw(this->currrentKickoffIndex, isRecording ? KICKOFF_ORANGE_SIDE : KICKOFF_BLUE_SIDE) * CONST_RadToUnrRot, 0));
		joueur->SetVelocity(Vector(0, 0, 0));
		BoostWrapper boost = joueur->GetBoostComponent();
		if (!boost)return;
		boost.SetBoostAmount(0.339f);
	}
	delete bot;
	delete joueur;
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
	for (int i = 0; i < loadedInputs.size();i++)
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
	ArrayWrapper<CarWrapper> tutures = server.GetCars();

	for (int i = 0; i < tutures.Count();i++)
	{
		if (tutures.Get(i).GetPRI().GetbBot())
		{
			tutures.Get(i).GetPRI().Unregister();
			tutures.Get(i).Destroy();
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
		LOG("Ticks recorded : {}",nbInputs);
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);
		
		std::ostringstream oss;
		oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
		auto str = oss.str();
		str = "\\recordedInput" + str + ".kstates";
		std::ofstream inputFile(recordedKickoffFolder  + str);
		if (!inputFile.is_open())
		{
			LOG("ERROR : can't create recording file");
			return;
		}
		for (int i = 0;i < nbInputs;i++)
		{
			const carState mvtJoueur = recordedInputs[i];
			inputFile << mvtJoueur.location.X << ", " << mvtJoueur.location.Y << ", " << mvtJoueur.location.Z 
			  << ", " << mvtJoueur.velocity.X << ", " << mvtJoueur.velocity.Y << ", " << mvtJoueur.velocity.Z 
			  << ", " << mvtJoueur.rotation.Pitch << ", " << mvtJoueur.rotation.Roll << ", " << mvtJoueur.rotation.Yaw
			  << ", " << mvtJoueur.angularVelocity.X << ", " << mvtJoueur.angularVelocity.Y << ", " << mvtJoueur.angularVelocity.Z
			  << ", " << mvtJoueur.steer
			  << ", " << mvtJoueur.throttle
			  << ", " << mvtJoueur.handbrakeOn
			  <<", " << mvtJoueur.boostAmount 
			  << ", " << mvtJoueur.isBoosting << "\n";
		}
		inputFile.close();
		this->recordedInputs = std::vector<carState>();
	}

	this->kickoffState= KickoffStates::nothing;
	this->tickCompteur = 0;
	this->currrentKickoffIndex = 0;
	this->resetBoost();
	this->isInReplay = false;
	this->isRecording = false;
}

void kickoffPractice::recordBoost()
{
	
	if (!gameWrapper->IsInFreeplay())return;
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server)return;
	if (this->isInReplay)return;
	auto joueur = gameWrapper->GetLocalCar();
	if (!joueur)return;
	auto boost = joueur.GetBoostComponent();
	if (!boost)return;
	this->unlimitedBoostDefaultSetting = boost.GetUnlimitedBoostRefCount();// /§\buggé
	//LOG("Boost settings recorded as {}",unlimitedBoostDefaultSetting);
}

void kickoffPractice::checkForKickoffDispo()
{
	kickoffDispo = std::vector<int>();
	for (int i = 0; i < states.size();i++)
	{
		if (states[i] == 0)continue;
		int vraiIndex = states[i] - 1;
		auto it = std::find(kickoffDispo.begin(), kickoffDispo.end(), vraiIndex);
		if (it == kickoffDispo.end())//si on l'a pas trouvé
		{
			kickoffDispo.push_back(vraiIndex);
		}
	}
}


void kickoffPractice::loadInputFiles()
{
	loadedInputs = std::vector<kickoffStates>();
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
	
	CarWrapper joueur = gameWrapper->GetLocalCar();
	if (!joueur)return;
	BoostWrapper boost = joueur.GetBoostComponent();
	if (!boost)return;
	//LOG("set unlimited boost settings as : {} ", unlimitedBoostDefaultSetting);
	boost.SetUnlimitedBoostRefCount(this->unlimitedBoostDefaultSetting);//TODO: ça remet toujours unlimited boost donc c buggé
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


kickoffStates kickoffPractice::readKickoffFile(std::string fileName, std::string kickoffName)
{
	std::vector<std::string> row;
	std::string line, word;
	std::vector<carState> currentInputs;

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
			//stockage dans la structure adéquate
			carState state;
			if (row.size() != 17)
			{
				LOG("Error on line {} : size of {} instead of 17",i,row.size());
				//LOG(line);

				continue;
			}
			/*for (int j = 0; j < row.size();j++)
			{
			cvarManager->log(row[j]);
			}*/
			try
			{
				state.location.X = std::stof(row[0]);
				state.location.Y = std::stof(row[1]);
				state.location.Z = std::stof(row[2]);
			
				state.velocity.X = std::stof(row[3]);
				state.velocity.Y = std::stof(row[4]);
				state.velocity.Z = std::stof(row[5]);
				
				state.rotation.Pitch = std::stoi(row[6]);
				state.rotation.Roll= std::stoi(row[7]);
				state.rotation.Yaw = std::stoi(row[8]);
			
				state.angularVelocity.X= std::stof(row[9]);
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
				LOG("ERROR : invalid argument in input file {}\n{}",fileName, exception.what());
			}
			catch (std::out_of_range exception)
			{
				LOG("ERROR : number too big in file {} \n{}",fileName, exception.what());
			}
		}
	}
	else
		LOG("Can't open {}",fileName);

	LOG("{} : {} lines loaded", fileName,currentInputs.size());
	kickoffStates a;
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
			/*for (int j = 0; j < row.size();j++)
			{
			cvarManager->log(row[j]);
			}*/
			try
			{
				std::string name = row[1];
				int state = std::stoi(row[0]);
				if (state < 0 || state > 5)continue;
				for (int j = 0; j < loadedInputs.size();j++)
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
				LOG("ERROR : number too big in config file \n{}",  exception.what());
			}
		}
	}
	else
		cvarManager->log("Can't open the config file");

	if (botFolder.size() < 128 && botFolder.size() > 0)
	{
		strcpy(this->botKickoffFolder,botFolder.c_str());
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

	for (int i = 0;i < states.size() ;i++)
	{
		inputFile << states[i]<< "," << this->loadedInputs[i].name << "\n";
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
	ImGui::SliderFloat("Time before back to normal", &this->timeAfterBackToNormal, 0.0f, 3.0f,"%.3f seconds");
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
				//LOG(selPath);
				strcpy(recordedKickoffFolder, selPath.c_str());
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PushID(42);//sinon ImGui confond les textboxs sans labels
	ImGui::InputText("", recordedKickoffFolder, IM_ARRAYSIZE(recordedKickoffFolder));
	ImGui::PopID();

#pragma endregion
	for (int i = 0; i < 5;i++)
	{
		if (ImGui::Button(("Record " + getKickoffName(i)).c_str()))
		{
			gameWrapper->Execute([this,i](GameWrapper* gw) {
				cvarManager->executeCommand("kickoff_train " + std::to_string(i+1) + " true;closemenu settings");
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(("Record input for the " + getKickoffName(i) + " kickoff").c_str());
		}
		if(i != 4)ImGui::SameLine();
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
		std::string selPath = recordMenu.main();
		if (selPath != "")
		{
			if (selPath.size() < 128)
			{
				//LOG(selPath);
				strcpy(botKickoffFolder, selPath.c_str());
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PushID(69);//sinon ImGui confond les textboxs sans labels
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

	int taille = loadedInputs.size();
	const char* items[] = { "Unused", "Right Corner", "Left Corner", "Back Right", "Back Left", "Far Back Center"};
	bool isChanged = false;
	if (child_is_visible) 
	{
		ImGui::Indent(5);
		for(int i = 0; i < taille;i ++)
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


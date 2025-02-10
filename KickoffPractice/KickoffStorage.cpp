#include "pch.h"
#include "KickoffStorage.h"

#include <set>

namespace fs = std::filesystem;

static const std::string CONFIG_FILE = "config.cfg";
static const std::string FILE_EXT = ".kinputs";

KickoffStorage::KickoffStorage(std::filesystem::path recordingDirectory)
{
	this->recordingDirectory = recordingDirectory;

	if (!fs::exists(recordingDirectory) || !fs::is_directory(recordingDirectory))
		if (!fs::create_directory(recordingDirectory))
			LOG("Can't create recording directory {}", recordingDirectory.string());
}

void KickoffStorage::saveRecording(RecordedKickoff& kickoff)
{
	auto filename = kickoff.name + FILE_EXT;
	std::ofstream inputFile(recordingDirectory / filename);
	if (!inputFile.is_open())
	{
		LOG("ERROR: can't create recording file");
		return;
	}

	inputFile << "position:" << Utils::toInt(kickoff.position) << "\n";
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

std::vector<RecordedKickoff> KickoffStorage::readRecordings()
{
	auto names = readActiveKickoffs();
	std::set<std::string> activeKickoffNames(names.begin(), names.end());
	auto isActive = [&](const RecordedKickoff& kickoff) { return 0 < activeKickoffNames.count(kickoff.name); };

	std::vector<RecordedKickoff> kickoffs;

	try
	{
		for (const auto& entry : fs::directory_iterator(recordingDirectory))
		{
			if (!entry.is_regular_file()) continue;
			if (entry.path().extension() != FILE_EXT) continue;

			auto kickoff = readRecording(entry.path());
			kickoff.isActive = isActive(kickoff);
			kickoffs.push_back(kickoff);
		}
	}
	catch (fs::filesystem_error const& ex)
	{
		LOG("ERROR: {}", ex.code().message());
	}

	return kickoffs;
}

RecordedKickoff KickoffStorage::readRecording(fs::path filePath)
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
						position = Utils::fromInt(std::stoi(row[0]));
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

void KickoffStorage::saveActiveKickoffs(std::vector<RecordedKickoff>& kickoffs)
{
	auto filename = recordingDirectory / CONFIG_FILE;

	std::ofstream inputFile(filename);

	if (!inputFile.is_open())
	{
		LOG("Can't create the config file");
		return;
	}

	for (auto& kickoff : kickoffs)
	{
		if (!kickoff.isActive) continue;
		inputFile << kickoff.name << "\n";
	}
	inputFile.close();
}

std::vector<std::string> KickoffStorage::readActiveKickoffs()
{
	auto filename = recordingDirectory / CONFIG_FILE;

	if (!fs::exists(filename))
		return {};

	std::fstream file(filename, std::ios::in);

	if (!file.is_open())
	{
		LOG("Can't open the config file");
		return {};
	}

	std::vector<std::string> activeKickoffNames;
	std::string line;

	while (getline(file, line))
	{
		activeKickoffNames.push_back(line);
	}
	file.close();

	return activeKickoffNames;
}

bool KickoffStorage::renameKickoffFile(std::string oldName, std::string newName)
{
	try
	{
		auto oldPath = recordingDirectory / (oldName + FILE_EXT);
		auto newPath = recordingDirectory / (newName + FILE_EXT);

		if (!fs::is_regular_file(oldPath))
		{
			LOG("No recording file found with this name: {}", oldName);
			return false;
		}
		if (fs::exists(newPath))
		{
			LOG("Already found a recording with this name: {}", newName);
			return false;
		}

		fs::rename(oldPath, newPath);
		return true;
	}
	catch (const fs::filesystem_error)
	{
		LOG("Failed to rename recording file");
		return false;
	}
}

bool KickoffStorage::deleteKickoffFile(std::string name)
{
	try
	{
		auto fileName = name + FILE_EXT;
		auto filePath = recordingDirectory / fileName;

		if (fs::exists(filePath) && !fs::is_regular_file(filePath))
		{
			LOG("Recording is no regular file: {}", name);
			return false;
		}

		fs::remove(filePath);
		return true;
	}
	catch (const fs::filesystem_error)
	{
		LOG("Failed to remove recording file");
		return false;
	}
}

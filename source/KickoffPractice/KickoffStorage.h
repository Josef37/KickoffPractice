#pragma once

#include <fstream>

#include "Common.h"

// Handles reading and writing kickoff recordings to and from files.
class KickoffStorage
{
public:
	void saveRecording(RecordedKickoff& kickoff);

	std::vector<RecordedKickoff> readRecordings();

	// Writes the names of all active/selected kickoffs to a file.
	void saveActiveKickoffs(std::vector<RecordedKickoff>& kickoffs);
	std::vector<std::string> readActiveKickoffs();

	bool renameKickoffFile(std::string oldName, std::string newName);
	bool deleteKickoffFile(std::string name);

	KickoffStorage(std::filesystem::path recordingDirectory);

private:
	std::filesystem::path recordingDirectory;

	RecordedKickoff readRecording(std::filesystem::path filePath);
};

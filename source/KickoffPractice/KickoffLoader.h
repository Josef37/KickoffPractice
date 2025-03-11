#pragma once

#include "Common.h"

class KickoffLoader
{
private:
	std::vector<std::shared_ptr<RecordedKickoff>> kickoffs;
	std::map<KickoffPosition, std::vector<std::shared_ptr<RecordedKickoff>>> kickoffsByPosition;
	std::map<std::string, std::shared_ptr<RecordedKickoff>> kickoffsByName;
	std::shared_ptr<RecordedKickoff> currentKickoff;

public:
	void clearLoadedKickoffs();
	void loadKickoff(std::shared_ptr<RecordedKickoff> kickoff);
	void loadKickoffs(std::vector<std::shared_ptr<RecordedKickoff>> kickoffs);
	void renameKickoff(std::string oldName, std::string newName);
	void unloadKickoff(std::string name);
	void setCurrentKickoff(std::shared_ptr<RecordedKickoff> kickoff);
	std::shared_ptr<RecordedKickoff> getCurrentKickoff();

	std::vector<std::shared_ptr<RecordedKickoff>> getKickoffs();
	std::vector<std::shared_ptr<RecordedKickoff>> getKickoffs(std::optional<GameMode> gameMode);
	std::vector<std::shared_ptr<RecordedKickoff>> getKickoffs(std::optional<GameMode> gameMode, KickoffPosition position);
	std::shared_ptr<RecordedKickoff> findKickoffByName(std::string name);
};
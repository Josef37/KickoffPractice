#include "pch.h"
#include "KickoffLoader.h"

void KickoffLoader::clearLoadedKickoffs()
{
	kickoffs.clear();
	kickoffsByPosition.clear();
	kickoffsByName.clear();
	setCurrentKickoff(nullptr);
}

void KickoffLoader::loadKickoff(std::shared_ptr<RecordedKickoff> kickoff)
{
	kickoffs.push_back(kickoff);
	kickoffsByName[kickoff->name] = kickoff;
	kickoffsByPosition[kickoff->position].push_back(kickoff);
}

void KickoffLoader::loadKickoffs(std::vector<std::shared_ptr<RecordedKickoff>> kickoffs)
{
	for (auto& kickoff : kickoffs)
		loadKickoff(kickoff);
}

void KickoffLoader::renameKickoff(std::string oldName, std::string newName)
{
	if (!kickoffsByName.contains(oldName)) return;
	if (kickoffsByName.contains(newName)) return;

	auto& kickoff = kickoffsByName[oldName];
	kickoff->name = newName;

	kickoffsByName[newName] = kickoff;
	kickoffsByName.erase(oldName);
}

void KickoffLoader::unloadKickoff(std::string name)
{
	if (!kickoffsByName.contains(name)) return;

	auto& kickoff = kickoffsByName[name];

	Utils::removeFromVector(kickoffs, kickoff);
	kickoffsByName.erase(kickoff->name);
	Utils::removeFromVector(kickoffsByPosition[kickoff->position], kickoff);

	if (kickoff == currentKickoff)
		setCurrentKickoff(nullptr);
}

void KickoffLoader::setCurrentKickoff(std::shared_ptr<RecordedKickoff> kickoff)
{
	currentKickoff = kickoff;
}

std::shared_ptr<RecordedKickoff> KickoffLoader::getCurrentKickoff()
{
	return currentKickoff;
}

std::vector<std::shared_ptr<RecordedKickoff>> KickoffLoader::getKickoffs()
{
	return kickoffs;
}

std::vector<std::shared_ptr<RecordedKickoff>> KickoffLoader::getKickoffs(std::optional<GameMode> gameMode)
{
	std::vector<std::shared_ptr<RecordedKickoff>> filteredKickoffs;

	for (auto& kickoff : kickoffs)
	{
		if (kickoff->gameMode == gameMode)
			filteredKickoffs.push_back(kickoff);
	}

	return filteredKickoffs;
}

std::vector<std::shared_ptr<RecordedKickoff>> KickoffLoader::getKickoffs(std::optional<GameMode> gameMode, KickoffPosition position)
{
	std::vector<std::shared_ptr<RecordedKickoff>> filteredKickoffs;

	for (auto& kickoff : kickoffsByPosition[position])
	{
		if (kickoff->gameMode == gameMode)
			filteredKickoffs.push_back(kickoff);
	}

	return filteredKickoffs;
}

std::shared_ptr<RecordedKickoff> KickoffLoader::findKickoffByName(std::string name)
{
	if (kickoffsByName.contains(name))
		return kickoffsByName[name];
	else
		return nullptr;
}

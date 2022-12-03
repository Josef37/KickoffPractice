#pragma once
#include <../IMGUI/imgui.h>
#include <filesystem>
#include <string>
#include <iostream>

class InputPath
{
private:
	static int compteur;
	std::filesystem::path currentFolder;
	std::string selectedPath ;
public:
	InputPath();
	std::string main();
};

#pragma once

#include <filesystem>
#include <string>
#include <iostream>

class InputPath
{
private:
	static int counter;
	std::filesystem::path currentFolder;
	std::string selectedPath;
public:
	InputPath();
	std::string main();
};

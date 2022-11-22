//#pragma once

#include <queue>
#include <string>
#include <filesystem>
#include <vector>
#include <exception>
#include <iostream>
#include "windows.h"

// simple exception just to know what happened
class our_check_flash_exception : std::exception
{
private:
	std::string name;

public:
	our_check_flash_exception() : name("Unknow ecxeption happened") {}
	our_check_flash_exception(std::string name_) : name(name_) {}
	std::string what() { return name; }
};

struct file
{
	std::string path;
	DWORD64 size;
	std::size_t seed, R;

	file(std::string path_, DWORD64 size_, std::size_t seed_, std::size_t R_) : path(path_), size(size_), seed(seed_), R(R_) {}
};

void check_the_flash();
void write_files(const std::vector<std::string>& folders_to_write, DWORD64 file_size, std::queue<file>& files_to_check);

// getting only paths, other parametres we define in other part
std::vector<std::string> initialize_file_tree(std::size_t depth, std::vector<std::string>& folders);

bool create_file(file& file);
bool check_file(file& file);

void clear_console();
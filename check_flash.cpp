//#include "c:\name\cpp_projects\flash_project\check_flash.h"
#include "check_flash.h"

#include <queue>
#include <string>
#include <filesystem>
#include <vector>
#include <fstream>
#include <iostream>
#include <random>
#include "windows.h"

namespace fs = std::filesystem;
using std::vector;
using std::string;
using std::size_t;
using std::queue;
using std::cout;
using std::cin;

// global variables are evil but they are needed in majority of functions
// using them will make our lives easier
DWORD start_file_size = 1e6; // start file size will be decreased a little in order to fully cover clusters
std::size_t files_in_directory = 100;
std::pair<unsigned char, unsigned char> byte_template; // will be set at the beginning of check_the_flash function
std::string start_directory; // will be set by user


void check_the_flash()
{
	// can use <bitset>
	// setting byte template 10101010 and 01010101
	for (int i = 0; i < sizeof(char); i++)
	{
		if (i % 2 == 0)
		{
			byte_template.first |= (1 << i); // making bit 1
		}
		else
		{
			byte_template.first &= ~(1 << i); // making bit 0
		}
	}
	byte_template.second = ~byte_template.first;

	DWORD
		SectorsPerCluster 		= 0,
		BytesPerSector 			= 0,
		NumberOfFreeClusters	= 0,
		TotalNumberOfClusters	= 0;
	cout << "Trying to get info about you flash driver...\n";
	// LPDWORD is a pointer to DWORD which is unsigned _int32, DWORD64 is unsigned long long
	// about this function https://docs.microsoft.com/ru-ru/windows/win32/api/fileapi/nf-fileapi-getdiskfreespacea?redirectedfrom=MSDN
	bool success = GetDiskFreeSpaceA(
		start_directory.c_str(),
		(LPDWORD)&SectorsPerCluster,
		(LPDWORD)&BytesPerSector,
		(LPDWORD)&NumberOfFreeClusters,
		(LPDWORD)&TotalNumberOfClusters);

	if (!success) // This problem can't be fixed, if it happened flash drive probably can't be rescued
	{
		cout << "Failed\n";
		cout << "Finishing the program\n";
		return;
	}

	cout << "Success\nInfo about you flash driver\n";
	cout << "Bytes per sector: " << BytesPerSector << "\n";
	cout << "Sectors per cluster: " << SectorsPerCluster << "\n";
	cout << "Number of free clusters: " << NumberOfFreeClusters << "\n";
	cout << "Total number of clusters: " << TotalNumberOfClusters << "\n\n";

	// used DWORD64 because DWORD can cover about 500mb, flash can easily have more free memory 
	// 1 cluster is big enough (usually more than 1kb) so we don't need to worry about overflow
	DWORD64	free_space = (DWORD64)BytesPerSector * (DWORD64)SectorsPerCluster * (DWORD64)NumberOfFreeClusters,
			min_file_size = (DWORD64)BytesPerSector * (DWORD64)SectorsPerCluster; // size of 1 cluster, can't be less
	cout << "Calculated free space: " << free_space << " bytes\n";
	cout << "Calculated size of one cluster: " << min_file_size << " bytes\n\n";

	if (start_file_size < min_file_size)
	{
		cout << "Start file will have size of one cluster as it is smaller\n";
		start_file_size = min_file_size;
	}

	start_file_size -= start_file_size % min_file_size;

	size_t tree_depth;
	// could be made in 1 stroke but it's very long
	if (free_space / start_file_size < files_in_directory)
	{
		tree_depth = 2; // root + heirs, 2 levels
	}
	else
	{
		// +1 because of root, c++ doesn't have direct logN(M) function
		// it was made according to an example of log at cpp.reference
		tree_depth = ceil(std::log(free_space / start_file_size) / std::log(files_in_directory)) + 1;
	}

	cout << "Initializing tree...\n";
	// write files in 'lowest_folders', 'folders' will be used to clear empty folders on a flash at the end
	vector<string> lowest_folders, folders;
	lowest_folders = initialize_file_tree(tree_depth, folders);
	cout << "Complete\n\n";

	queue<file> files_to_check;
	try
	{
		write_files(lowest_folders, start_file_size, files_to_check);

		vector<string> to_delete;
		while (!files_to_check.empty())
		{
			clear_console();
			cout << "Checking files...\n";

			file file = files_to_check.front();
			files_to_check.pop();

			if (!fs::exists(file.path))
			{
				throw our_check_flash_exception("file unexpectedly disappeared"); // file disappeared, it's not ok
			}
			if (fs::file_size(file.path) != file.size)
			{
				throw our_check_flash_exception("file was changed or damaged"); // file came to the check damaged
				// it means flash can't be 'rescued' by means of this program
			}

			bool file_correct = check_file(file);
			if (file_correct)
			{
				to_delete.push_back(file.path); // delete at the end of processing
			}
			else
			{
				if (file.size == min_file_size)
				{
					continue; // leave this file on disk
				}

				clear_console();
				cout << "Incorrect file was found\n";
				cout << "Start processing\n";
				// file can't take less space than size of 1 cluster
				// if calculated new size less than size of 1 cluster we use size of 1 cluster
				// last file can be bigger than others
				DWORD64 new_file_size = std::max(min_file_size, file.size / files_in_directory);
				new_file_size -= new_file_size % min_file_size;

				// replacing file with folder with smaller files
				fs::remove(file.path);
				fs::create_directory(file.path);

				write_files({file.path}, new_file_size, files_to_check);
			}
		}

		// deleting rubbish left
		// if we get here no files or folders disappeared
		clear_console();
		cout << "Check complete\n\n";
		cout << "Removing unnecessary files...\n";
		for (auto& file : to_delete)
		{
			fs::remove(file);
		}
		cout << "Complete\n\n";
		cout << "Removing empty folders...\n";
		// removing empty folders
		// folders are wtitten in the way lowest folders go after folders before then
		for (int i = folders.size() - 1; i >= 0; i--)
		{
			if (fs::is_empty(folders[i]))
			{
				fs::remove(folders[i]);
			}
		}
		cout << "Complete\n";
	}
	// exceptions do the same thing
	catch (our_check_flash_exception& e)
	{
		cout << e.what() << "\n";
		cout << "Removing all changes\n";
		if (fs::exists(folders[0]))
		{
			fs::remove(folders[0]);
		}
	}
	catch (std::exception& e)
	{
		cout << e.what() << "\n";
		cout << "Removing all changes\n";
		if (fs::exists(folders[0]))
		{
			fs::remove(folders[0]);
		}
	}
	catch (...)
	{
		cout << "Unknow exception happened\n";
		cout << "Removing all changes\n";
		if (fs::exists(folders[0]))
		{
			fs::remove(folders[0]);
		}
	}
}


void write_files(const vector<string>& folders_to_write, DWORD64 file_size, queue<file>& files_to_check)
{
	// better to keep info about free space updated
	// in one of tested flashes it unexpectedly changed (btw it was almost dead flash)
	DWORD
		SectorsPerCluster		= 0,
		BytesPerSector			= 0,
		NumberOfFreeClusters	= 0,
		TotalNumberOfClusters	= 0;
	GetDiskFreeSpaceA(
		start_directory.c_str(),
		(LPDWORD)&SectorsPerCluster,
		(LPDWORD)&BytesPerSector,
		(LPDWORD)&NumberOfFreeClusters,
		(LPDWORD)&TotalNumberOfClusters);
	DWORD64 free_space = (DWORD64)BytesPerSector * (DWORD64)SectorsPerCluster * (DWORD64)NumberOfFreeClusters,
			start_free_space = free_space;

	int64_t idx = 0;

	clear_console();
	cout << "Writing files on flash drive...\n\n";
	cout << "Bytes per sector: " << BytesPerSector << "\n";
	cout << "Sectors per cluster: " << SectorsPerCluster << "\n";
	cout << "Number of free clusters: " << NumberOfFreeClusters << "\n";
	cout << "Total number of clusters: " << TotalNumberOfClusters << "\n\n";
	cout << "Progress: 0%\n";

	for (auto& folder : folders_to_write)
	{
		for (int file_number = 1; file_number <= files_in_directory && free_space >= file_size; file_number++)
		{
			file file(folder + "\\" + std::to_string(file_number), file_size, rand(), RAND_MAX / 2);
			
			if (!fs::exists(folder))
			{
				throw our_check_flash_exception("folder disappeared"); // folder disappeared
				// it can sound strange but in the test with almost dead flash this situation happened
			}
			bool file_created = create_file(file);
			if (!file_created)
			{
				throw our_check_flash_exception("unable to create file"); // file wasn't created, this exception will never happen
			}
			if (fs::file_size(file.path) != file.size)
			{
				throw our_check_flash_exception("file was written incorrectly"); // file was written incorrectly
			}
			files_to_check.push(file);

			// updating console text and free space
			GetDiskFreeSpaceA(
				start_directory.c_str(),
				(LPDWORD)&SectorsPerCluster,
				(LPDWORD)&BytesPerSector,
				(LPDWORD)&NumberOfFreeClusters,
				(LPDWORD)&TotalNumberOfClusters);
			free_space = (DWORD64)BytesPerSector * (DWORD64)SectorsPerCluster * (DWORD64)NumberOfFreeClusters;

			clear_console();
			cout << "Writing files on flash drive...\n\n";
			cout << "Bytes per sector: " << BytesPerSector << "\n";
			cout << "Sectors per cluster: " << SectorsPerCluster << "\n";
			cout << "Number of free clusters: " << NumberOfFreeClusters << "\n";
			cout << "Total number of clusters: " << TotalNumberOfClusters << "\n\n";
			cout << "Progress: " << 100 - 100 * free_space / start_free_space << "%\n";
		}
	}
	if (free_space > 0)
	{
		file file(folders_to_write.back() + "\\" + std::to_string(files_in_directory + 1), free_space, rand(), RAND_MAX / 2);
		
		if (!fs::exists(folders_to_write.back()))
		{
			throw our_check_flash_exception("folder disappeared"); // folder disappeared
		}
		bool file_created = create_file(file);
		if (!file_created)
		{
			throw our_check_flash_exception("unable to create file"); // file wasn't created
		}
		if (fs::file_size(file.path) != file.size)
		{
			throw our_check_flash_exception("file was written incorrectly"); // file was written incorrectly
		}
		files_to_check.push(file);
	}
}

// after this function imitation of file tree will be created on a disk
vector<string> initialize_file_tree(size_t depth, vector<string>& folders)
{
	// finding name which doesn't conflict with existing folder names
	int tmp = 0;
	while (fs::exists(start_directory + "treebase" + std::to_string(tmp)))
	{
		tmp++;
	}
	string current_path = start_directory + "treebase" + std::to_string(tmp);
	folders.push_back(current_path); // in folders will be added all folders of the tree

	fs::create_directory(current_path);
	vector<string> lowest_folders = {current_path}; // contains ONLY folders in lowest layers

	// initializing is organized layer by layer
	// the first layer is already created
	// the last layer will be filled by files, it won't be created here
	for (size_t current_lvl = 1; current_lvl < depth - 1; current_lvl++)
	{
		for (size_t i = 0; i < std::pow(files_in_directory, current_lvl - 1); i++) // number of folders in prev lvl
		{
			for (size_t number_in_folder = 1; number_in_folder <= files_in_directory; number_in_folder++)
			{
				current_path = lowest_folders[0] + "\\" + std::to_string(number_in_folder);
				fs::create_directory(current_path);

				lowest_folders.push_back(current_path); // add to the end
				folders.push_back(current_path);
			}
			lowest_folders.erase(lowest_folders.begin()); // it is no more in lowest lvl as it has heirs
		}
	}
	return lowest_folders;
}


bool create_file(file& file)
{
	if (fs::exists(file.path))
		return false;

	std::ofstream fout(file.path.c_str());
	if (fout)
	{
		DWORD64 size = file.size;
		srand(file.seed);

		for (DWORD64 i = 0; i < size; i++) // writing 1 byte every time
		{
			if (rand() <= file.R)
				fout << byte_template.first;
			else
				fout << byte_template.second;
		}
		fout.close();
		return true;
	}
	return false;
}


bool check_file(file& file)
{
	std::ifstream fin(file.path.c_str());
	if (fin)
	{
		DWORD64 size = file.size;
		srand(file.seed);

		unsigned char byte_current;
		bool file_correct = true;
		for (DWORD64 i = 0; i < size && file_correct; i++)
		{
			fin >> byte_current;
			if (rand() <= file.R)
				file_correct &= byte_current == byte_template.first;
			else
				file_correct &= byte_current == byte_template.second;
		}
		fin.close();
		return file_correct;
	}
	return false; // unable to read
}


void clear_console()
{
	cout << std::flush;
	system("clear");
}


int main()
{
	bool to_continue = true, to_finish = false;
	string value_got, message = "";

	// after this cycle we get correct flash driver path
	while (to_continue && !to_finish)
	{
		clear_console();

		cout << "Flash desructor program is greeting you\n";
		cout << "To exit enter exit\n\n";
		cout << message;
		cout << "Enter flash drive path\n";

		cin >> value_got;
		if (value_got == "exit")
		{
			to_finish = true;
		}
		else
		{
			if (fs::exists(value_got))
			{
				// checking that we got the flash driver name
				// other types of drives can't pass this check
				if (GetDriveTypeA(value_got.c_str()) != 2)
				{ 
					message = "Disk " + value_got + " is not a flash drive\n\n";
				}
				else
				{
					to_continue = false;
				}
			}
			else
			{
				message = "The flash drive " + value_got + " wasn't found\n";
				message += "Make sure you don't use it and didn't miss \\\\\n\n"; // looks scary
			}
		}
	}
	if (!to_finish)
	{
		clear_console();
		size_t number;
		cout << "Enter number of files in directory or 0 to use default: \n";
		cin >> number;
		if (number)
		{
			if (number < 10)
			{
				cout << "Authors of the program don't allow use of less than 10 files in directory\n";
				cout << "Will be used default value\n";
				files_in_directory = 100;
			}
			else
			{
				files_in_directory = number;
			}
		}
		DWORD size;
		cout << "Enter approximate size of start file in bytes or 0 to use default: \n";
		cin >> size;
		if (size)
		{
			start_file_size = size;
		}
		start_directory = value_got;
		clear_console();
		check_the_flash();
	}
	//system("pause");
	return 0;
}

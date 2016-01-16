#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <experimental\filesystem>

#define BIOSHOCK "Bioshock.exe"
#define BACKUP "Bioshock_Backup.exe"
#define TIMESTEP_SIGNATURE "\x48\x6B\x55\x6E\x72\x65\x61\x6C\x43\x68\x61\x72\x61\x63\x74\x65\x72\x50\x6F\x69\x6E\x74\x43\x6F\x6C\x6C\x65\x63\x74\x6F\x72\x40\x40\x00\x00\x00\x3A\x3A\x3A\x3A\x3A\x3A\x3A\x3A\x00\x00\x00\x00\x2E\x3F\x41\x56\x46"
#define TIMESTEP_MASK "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx???????????????xxxxx"
#define TIMESTEP_OFFSET 36
#define MESSAGE_SLEEP_TIME 2500
#define ERROR_SLEEP_TIME 7000

struct ScanResult {
	bool found;
	std::streampos location;
};

ScanResult scanPatchLocation(std::fstream &file, const char *search, const char *mask, unsigned int offset) {
	std::stringstream strings;
	strings << file.rdbuf();
	std::string fileString = strings.str();
	
	ScanResult patch;
	size_t fileLength = fileString.length();
	size_t sigLength = strlen(mask);

	for (unsigned int i = 0; i < fileLength - sigLength; i++) {
		for (unsigned int j = 0; j < sigLength; j++) {
			if (mask[j] == '?') { continue; }
			if (search[j] == fileString.at(i + j)) {
				if (j == sigLength - 1) {
					patch.found = true;
					patch.location = i + offset;
					return patch;
				}
			}
			else {
				break;
			}
		}
	}

	patch.found = false;
	patch.location = 0;
	return patch;
}

int main() {
	// Try to locate Bioshock
	std::cout << "Opening Bioshock executable...";
	std::fstream bsBinary(BIOSHOCK, std::ios::binary | std::ios::out | std::ios::in);

	if (!bsBinary.good()) {
		std::cerr << "Error! Could not find the Bioshock executable. Aborting..." << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(ERROR_SLEEP_TIME));
		return EXIT_FAILURE;
	}
	else {
		std::cout << "Success!" << std::endl;
	}

	// Find the location in the exe to patch
	std::cout << "Scanning for location to patch...";
	ScanResult timestepPatch = scanPatchLocation(bsBinary, TIMESTEP_SIGNATURE, TIMESTEP_MASK, TIMESTEP_OFFSET);
	if (!timestepPatch.found) {
		std::cerr << "Error! Could not find the physics timestep location to patch. Aborting..." << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(ERROR_SLEEP_TIME));
		return EXIT_FAILURE;
	}
	else {
		std::cout << "Success!" << std::endl;
	}

	// Make a backup if we don't already have one.
	if (!std::experimental::filesystem::exists(BACKUP)) {
		std::cout << "Making a backup of Bioshock.exe...";

		std::ofstream saveBackup(BACKUP, std::ios::binary);
		if (saveBackup.good()) {
			bsBinary.clear();
			bsBinary.seekg(0, std::ios::beg);
			saveBackup << bsBinary.rdbuf();
			std::cout << "Success!" << std::endl;
		}
		else {
			std::cerr << "WARNING: Could not create backup!" << std::endl;
		}
	}

	// Show the user what the timestep is set to right now.
	float curTimestep;
	bsBinary.seekg(timestepPatch.location);
	bsBinary.read((char*)&curTimestep, sizeof(float));
	std::cout << "The physics FPS is currently: " << (1.0 / curTimestep) << " FPS" << std::endl;

	// User input so they can change it.
	std::string inputLine;
	int inFPS;
	while ((std::cout << "New physics FPS: ") && std::getline(std::cin, inputLine) && !(std::istringstream{ inputLine } >> inFPS)) {
		std::cerr << "Invalid choice." << std::endl;
	}

	// Actually replace it in the file
	float desiredTimestep = 1.0f / inFPS;
	bsBinary.seekp(timestepPatch.location);
	bsBinary.write((char*)&desiredTimestep, sizeof(float));

	// cool.
	std::cout << "Updated physics to " << inFPS << " FPS! Quitting..." << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(MESSAGE_SLEEP_TIME));

	return EXIT_SUCCESS;
}
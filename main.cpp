#ifdef _WIN32
// #define _WINSOCKAPI_
// #include <winsock2.h>
// #include <windows.h>
#else
#include <X11/Xlib.h>
#endif
#include <string>
#include <iostream>
#include "Atmosphere.hpp"

#include <fstream>
#include <chrono>

void LogToFile(const std::string& message)
{
#ifdef _WIN32
	std::string logpath = "C:\\Temp\\Cake_screensaver.log";
#else
	std::string logpath = "/tmp/Cake_screensaver.log";
#endif
	try {
		std::ofstream file(logpath, std::ios::app);
		if (!file.is_open()) return;
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		file << "[" << std::ctime(&time) << "] " << message << "\n";
		file.close();
	} catch (...) { }
}

int main() {

	std::cout << "Hiii" << std::endl;

	try {
		LogToFile("Creating Atmosphere game instance...");
		Atmosphere game;

		LogToFile("Running game...");
		game.Run();

		LogToFile("Game exited normally");
	} catch (const std::exception& e) {
		LogToFile(std::string("Exception caught: ") + e.what());
	} catch (...) {
		LogToFile("Unknown exception caught");
	}

	return 0;
}

#ifdef _WIN32

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
	return main();
}

#endif

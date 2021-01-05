#ifndef __UTILITES__
#define __UTILITES__

#include <string>
#include <vector>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

std::string getCmdOption(char ** begin, char ** end, const std::string & option);
bool cmdOptionExists(char** begin, char** end, const std::string& option);

std::string InLower(const std::string &inHigher);

void DrawProgressBar(size_t done, size_t all, size_t bar_width = 50);

void StartTimer();
double StopTimer();
std::string StopTimerInNormalTime();

void CopyRemove(const std::string& from, const std::string& to);
void RecursiveCopy(const std::string& from, const std::string& to, const std::string& prefix);

void JoinThreads();

size_t GetNumOfFilesInDirrectory(const fs::path& path);

#endif
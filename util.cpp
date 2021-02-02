#include "util.h"
#include <iostream>
#include <chrono>
#include <locale>
#include <fstream>

std::vector<std::thread> vWorkerThreads;

size_t filesCount = 0;
size_t filesPassed = 0;
size_t diffCounter = 0;
size_t errorCounter = 0;

std::ofstream logFile;

std::chrono::steady_clock::time_point prvTime;

extern bool bEnableLogging;
extern bool bEnableProgressBar;
extern bool bUseOptRes;

std::string getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return std::string(*itr);
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

std::string InLower(const std::string &inHigher)
{
    std::string strcopy(inHigher);
    std::transform(strcopy.begin(), strcopy.end(), strcopy.begin(), [](unsigned char c){ return std::tolower(c); });
    return strcopy;
}

void DrawProgressBar(size_t done, size_t all, size_t bar_width)
{
    double progress = (double)done/(double)all;
    std::cout << "[";
    int pos = bar_width * progress;
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << '0' + 219;
        else if (i == pos) std::cout << '0' + 220;
        else std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << " %\r";
    std::cout.flush();
}

void StartTimer()
{
    prvTime = std::chrono::steady_clock::now();
}

double StopTimer()
{
    std::chrono::steady_clock::time_point curTime = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double> >(curTime - prvTime).count();
}

std::string StopTimerInNormalTime()
{
    std::string result;
    double elapsed = StopTimer();
    return result.append(std::to_string((int)elapsed/60)).append("m ").append(std::to_string((int)elapsed%60)).append("s ");
}

void CopyRemove(const std::string& from, const std::string& to)
{
    if(fs::exists(to)) 
    {
        if(bEnableLogging) logFile << "INFO: " << "Updating file " << to << std::endl;
        fs::remove(to);
    }

    try
    {
        fs::copy(from, to);
    }
    catch(const std::exception& e)
    {
        if(bEnableLogging) logFile << "ERROR: " << e.what() << std::endl;
        errorCounter++;
    }
}

void RecursiveCopy(const std::string& from, const std::string& to, const std::string& prefix)
{
    for (const auto& entry : fs::directory_iterator(from)) 
    {
        if (entry.is_directory()) 
        {
            if(bEnableProgressBar) DrawProgressBar(filesPassed, filesCount);
            if(vWorkerThreads.size() != 0) JoinThreads();
            RecursiveCopy(entry.path().string(), to, prefix);
        }
        else if (entry.is_regular_file())
        {
            if(bEnableProgressBar) filesPassed++;
            if(entry.path().extension().string() == ".flv" && bUseOptRes)
            {
                if(bEnableLogging) logFile << "INFO: " << "File ignored: " << entry.path() << std::endl;
            }
            else
            {
                std::string sCurPath = entry.path().string();
                std::string sRealFolderPath = to + entry.path().parent_path().string().erase(0, prefix.size());
                std::string filepath = InLower(sRealFolderPath) + SLASHES + InLower(entry.path().filename().string());
                //TODO: make it parallel
                fs::create_directories(InLower(sRealFolderPath));
                vWorkerThreads.emplace_back(CopyRemove, entry.path().string(), filepath);
            }
        }
        else
            if(bEnableLogging) logFile << "INFO: " << "Unknown file" << " [?]" << entry.path() << std::endl;
    }
}

void JoinThreads()
{
    for(std::thread & th : vWorkerThreads)
    {
        if(th.joinable())
            th.join();
    }
    vWorkerThreads.clear();
}

size_t GetNumOfFilesInDirrectory(const fs::path& path)
{
    size_t counter = 0;
    for(auto& p: fs::recursive_directory_iterator(path))
        if(p.is_regular_file()) counter++;

    return counter;
}
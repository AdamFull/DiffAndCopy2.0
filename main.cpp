#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include "sha256.h"

#include "json11/json11.hpp"
//Json parser lib: https://github.com/dropbox/json11

#ifndef JSON11_ENABLE_DR1467_CANARY
#define JSON11_ENABLE_DR1467_CANARY 0
#endif

namespace fs = std::filesystem;

#define VERSION "version 2.0"

//TODO: Load this data from .ini
std::string sDefaultInPath = "";
std::string sDefaultOutpPath = "";

std::string sRefDir = "";
std::string sTargetSubDir = "";
std::string sOutSubDir = "";

std::string sConfigPath = "config.json";

std::vector<std::string> inputLocals = { "RU", "CS", "DE", "FR", "NL", "JP" };
std::vector<std::string> outputLocals = { "data_ru", "data_cs", "data_de", "data_fr", "data_nl", "data_jp" };

bool bEnableLogging = true;
bool bEnableProgressBar = false;
bool bIgnoreFlv = false;

std::chrono::steady_clock::time_point prvTime;

typedef struct {
    fs::path fpath;
    fs::path dpath;
    std::string fname;
    std::string fhash;
    size_t fSize;
} node;

std::vector<std::thread> vWorkerThreads;
std::mutex mReadProtect;

std::ofstream logFile;

size_t filesCount = 0;
size_t filesPassed = 0;
size_t diffCounter = 0;

//Walk dirrectory
//Parallelize all dirrectory reading
    //If file
        //Get path
        //Get parent dir
        //Get file name
        //Get file hash
        //Get file size
    //Check file sizes
    //If same
    //Check file hashes
    //If same, skip
    //If files have difference save to folder
//End

/*******************************************************************************************/
/*****************************************UTILS*********************************************/
/*******************************************************************************************/
std::string getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return std::string(*itr);
    }
    return 0;
}

/*******************************************************************************************/
bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

/*******************************************************************************************/
void DrawProgressBar(size_t done, size_t all, size_t bar_width = 50)
{
    double progress = (double)done/(double)all;
    std::cout << "[";
    int pos = bar_width * progress;
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << (char)219;
        else if (i == pos) std::cout << (char)220;
        else std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << " %\r";
    std::cout.flush();
}

/*******************************************************************************************/
void StartTimer()
{
    prvTime = std::chrono::steady_clock::now();
}

/*******************************************************************************************/
double StopTimer()
{
    std::chrono::steady_clock::time_point curTime = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double> >(curTime - prvTime).count();
}

/*******************************************************************************************/
bool startWith(std::string const & srString, std::string const & srStartString)
{
    if( srString.length() >= srStartString.length() )
        return (0 == srString.compare(0, srStartString.length(), srStartString));
    else
        return false;
}

/*******************************************************************************************/
/***************************************File work*******************************************/
/*******************************************************************************************/
node GetFileParams(const fs::path entry)
{
    node newNode;
    std::ifstream fsCurFile;
    newNode.fpath = entry;
    newNode.fname = entry.filename().string();
    newNode.dpath = newNode.fpath.parent_path();

    fsCurFile.open(newNode.fpath, std::ios_base::in | std::ios_base::binary);
    newNode.fhash = sha256(std::string((std::istreambuf_iterator<char>(fsCurFile)), std::istreambuf_iterator<char>()));
    fsCurFile.close();
    
    return newNode;
}

/*******************************************************************************************/
void CheckDiffAndCopy(std::string rHash, std::string sTarget, std::string inLoc, std::string outLoc)
{
    node nTarget = GetFileParams(fs::path(sTarget));
    if(rHash != nTarget.fhash)
    {
        
        std::string outPath = sDefaultOutpPath + "\\" + sOutSubDir + "\\" + outLoc + "\\" + sTargetSubDir;
        std::string outPathPrefix = sDefaultInPath + "\\" + inLoc + "\\" + sTargetSubDir;
        std::string sRealFolderPath = outPath + nTarget.dpath.string().erase(0, outPathPrefix.size());
        fs::path realFolderPath = fs::path(sRealFolderPath);

        //mReadProtect.lock();
        try
        {
            fs::create_directories(realFolderPath);
            fs::copy(nTarget.fpath, realFolderPath);
            diffCounter++;
        }
        catch(...)
        {
            if(bEnableLogging) logFile << "Error: " << "can't write file" << std::endl;
        }
        //mReadProtect.unlock();
    }
}

/*******************************************************************************************/
void ReadDirrectory(fs::path srSearchPath)
{
    for (const auto& entry : fs::directory_iterator(srSearchPath)) 
    {
        const auto filenameStr = entry.path().filename().string();
        if (entry.is_directory()) 
        {
            if(bEnableLogging) logFile << "In folder " << entry.path().string() << " found " << diffCounter << " diffs" << std::endl;
            diffCounter = 0;
            ReadDirrectory(entry);
            if(bEnableProgressBar) DrawProgressBar(filesPassed, filesCount);
        }
        else if (entry.is_regular_file())
        {
            node nReference = GetFileParams(entry.path());
            for(size_t i = 0; i < inputLocals.size(); i++)
            {
                std::string sCurPath = entry.path().string();
                std::string sForReplace = "\\" + inputLocals[i] + "\\";
                std::string sSubstr = std::string("\\" + sRefDir + "\\");
                sCurPath.replace(sCurPath.find(sSubstr), sSubstr.length(), sForReplace);
                fs::path pCurPath = fs::path(sCurPath);
                if(bEnableProgressBar) filesPassed++;
                if(fs::exists(pCurPath))
                    vWorkerThreads.emplace_back(CheckDiffAndCopy, nReference.fhash, sCurPath, inputLocals.at(i), outputLocals.at(i));
            }
        }
        else
            if(bEnableLogging) logFile << "Unknown file" << " [?]" << filenameStr << std::endl;
    }
}

/*******************************************************************************************/
void CheckInputPaths(std::vector<std::string> * inputsCheck, std::vector<std::string> * outputsCheck)
{
    for(size_t i = 0; i < inputsCheck->size(); i++)
    {
        fs::path curPath = fs::path(sDefaultInPath + "\\" + inputsCheck->at(i) + "\\" + sTargetSubDir);
        if(!fs::exists(curPath))
        {
            inputsCheck->erase(inputsCheck->begin() + i);
            outputsCheck->erase(outputsCheck->begin() + i);
        }
    }
}

/*******************************************************************************************/
bool ReadConfiguration()
{
    std::ifstream jsonConfig;
    #ifdef Debug
    jsonConfig.open("..\\..\\config.json");
    #else
    jsonConfig.open(sConfigPath);
    #endif

    if(jsonConfig)
    {
        std::string jsonData = std::string((std::istreambuf_iterator<char>(jsonConfig)), std::istreambuf_iterator<char>());
        std::string err;
        const auto json = json11::Json::parse(jsonData, err);
        if(!json.is_null())
        {
            if(sDefaultInPath.empty())
                sDefaultInPath = json["default_input_path"].string_value();
            if(sDefaultOutpPath.empty())
                sDefaultOutpPath = json["default_output_path"].string_value();
            
            if(sRefDir.empty())
                sRefDir = json["reference_dirrectory"].string_value();
            if(sTargetSubDir.empty())
                sTargetSubDir = json["target_subdirrectory"].string_value();
            if(sOutSubDir.empty())
                sOutSubDir = json["output_subdirrectory"].string_value();

            inputLocals.clear();
            for(auto &it : json["input_locals"].array_items())
            {
                inputLocals.push_back(it.string_value());
            }

            outputLocals.clear();
            for(auto &it : json["output_locals"].array_items())
            {
                outputLocals.push_back(it.string_value());
            }

            bEnableLogging = json["enable_logging"].bool_value();
            bEnableProgressBar = json["enable_progressbar"].bool_value();
            bIgnoreFlv = json["ignore_flv"].bool_value();
            
            jsonConfig.close();
            return true;
        }
    }
    return false;
}

/*******************************************************************************************/
size_t GetNumOfFilesInDirrectory(std::filesystem::path path)
{
    return (std::size_t)std::distance(fs::recursive_directory_iterator{path}, fs::recursive_directory_iterator{});
}

/*******************************************************************************************/
int main(int argc, char * argv[])
{
    if(argc > 1)
    {
        if(cmdOptionExists(argv, argv+argc, "-i"))
            sDefaultInPath = getCmdOption(argv, argv+argc, "-i");

        if(cmdOptionExists(argv, argv+argc, "-o"))
            sDefaultOutpPath = getCmdOption(argv, argv+argc, "-o");

        if(cmdOptionExists(argv, argv+argc, "-rd"))
            sRefDir = getCmdOption(argv, argv+argc, "-rd");

        if(cmdOptionExists(argv, argv+argc, "-sd"))
            sTargetSubDir = getCmdOption(argv, argv+argc, "-sd");

        if(cmdOptionExists(argv, argv+argc, "-od"))
            sOutSubDir = getCmdOption(argv, argv+argc, "-od");

        if(cmdOptionExists(argv, argv+argc, "-cp"))
            sConfigPath = getCmdOption(argv, argv+argc, "-cp");
        
        if(cmdOptionExists(argv, argv+argc, "-h"))
        {
            std::cout << "-i Input dirrectory path." << std::endl;
            std::cout << "-o Output dirrectory path." << std::endl;
            std::cout << "-rd Reference folder name." << std::endl;
            std::cout << "-sd Target subdirrectory. Directory inside reference folder." << std::endl;
            std::cout << "-od Output subdirrectory. Directory where program will save output files." << std::endl;
            std::cout << "-cp Config file path." << std::endl;
        }
    }
    else
        std::cout << "No arguments found. Reading from config" << std::endl;

    StartTimer();

    if(!ReadConfiguration())
    {
        std::cout << "Bad configuration." << std::endl;
        return 0;
    }

    if(sDefaultInPath.empty() || sDefaultInPath.empty())
        std::cout << "Bad input or output dirrectory." << std::endl;

    //TODO: Delete report if already exists
    if(bEnableLogging) logFile.open("DiffAndCopyReport.txt", std::ios_base::out | std::ios_base::app);

    CheckInputPaths(&inputLocals, &outputLocals);

    std::cout << "Checking difference..." << std::endl;
    fs::path curPath = fs::path(sDefaultInPath + "\\" + sRefDir + "\\" + sTargetSubDir);

    if(!fs::exists(curPath))
    {
        std::cout << "Input path not found." << std::endl;
        return 0;
    }

    if(bEnableProgressBar) filesCount =  GetNumOfFilesInDirrectory(curPath) * inputLocals.size();
    ReadDirrectory(curPath);

    for(std::thread & th : vWorkerThreads)
    {
        if(th.joinable())
            th.join();
    }
    vWorkerThreads.clear();

    //TODO: Check is already exists
    std::cout << std::endl << "Copying EN folder." << std::endl;
    StartTimer();
    std::string referenceOut = sDefaultOutpPath + "\\" + sOutSubDir + "\\" + sTargetSubDir;
    fs::create_directories(referenceOut);
    fs::copy(curPath, referenceOut, std::filesystem::copy_options::recursive);
    std::cout << "Operation time: " << StopTimer() << "s" << std::endl;
    if(bEnableLogging) logFile.close();

    return 0;
}
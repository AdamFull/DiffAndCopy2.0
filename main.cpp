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

#define DEBUG

namespace fs = std::filesystem;

#define VERSION "version 2.0"

//TODO: Load this data from .ini
std::string sDefaultInPath = "E:\\svn\\game";
std::string sDefaultOutpPath = "E:\\svn\\output";

std::string sRefDir = "EN";
std::string sTargetSubDir = "data";
std::string sOutSubDir = "res";

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

void StartTimer()
{
    prvTime = std::chrono::steady_clock::now();
}

double StopTimer()
{
    std::chrono::steady_clock::time_point curTime = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double> >(curTime - prvTime).count();
}

bool startWith(std::string const & srString, std::string const & srStartString)
{
    if( srString.length() >= srStartString.length() )
        return (0 == srString.compare(0, srStartString.length(), srStartString));
    else
        return false;
}

node GetFileParams(const fs::path entry)
{
    node newNode;
    std::ifstream fsCurFile;
    newNode.fpath = entry;
    newNode.fname = entry.filename().string();
    newNode.dpath = newNode.fpath.parent_path();

    if(entry.extension() == ".png")
    {
        fsCurFile.open(newNode.fpath, std::ios_base::in | std::ios_base::binary | std::ios::ate);

        if(fsCurFile)
        {
            char* pngdata = 0;
            size_t pngsize = fsCurFile.tellg();

            pngdata = new char[pngsize + 1];
            fsCurFile.read(pngdata, pngsize);
            newNode.fhash = sha256(pngdata);
            delete [] pngdata;
        }
        fsCurFile.close();
    }
    else
    {
        fsCurFile.open(newNode.fpath);
        newNode.fhash = sha256(std::string((std::istreambuf_iterator<char>(fsCurFile)), std::istreambuf_iterator<char>()));
        fsCurFile.close();
    }
    
    return newNode;
}

//Get file stats
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
                //TODO: Add my progress bar
            }
        }
        else
            if(bEnableLogging) logFile << "Unknown file" << " [?]" << filenameStr << std::endl;
    }
}

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

void ReadConfiguration()
{
    std::ifstream jsonConfig;
    #ifdef DEBUG
    jsonConfig.open("..\\..\\config.json");
    #else
    jsonConfig.open("config.json");
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

            sRefDir = json["reference_dirrectory"].string_value();
            sTargetSubDir = json["target_subdirrectory"].string_value();
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
            //inputLocals = ;
            //outputLocals = ;

            bEnableLogging = json["enable_logging"].bool_value();
            bEnableProgressBar = json["enable_progressbar"].bool_value();
            bIgnoreFlv = json["ignore_flv"].bool_value();
        }
        jsonConfig.close();
    }
}

size_t GetNumOfFilesInDirrectory(std::filesystem::path path)
{
    return (std::size_t)std::distance(fs::recursive_directory_iterator{path}, fs::recursive_directory_iterator{});
}

int main(int argc, const char * argv[])
{
    //TODO: add reading arguments
    StartTimer();
    ReadConfiguration();

    //TODO: Delete report if already exists
    if(bEnableLogging) logFile.open("DiffAndCopyReport.txt", std::ios_base::out | std::ios_base::app);

    CheckInputPaths(&inputLocals, &outputLocals);

    std::cout << "Checking difference..." << std::endl;
    fs::path curPath = fs::path(sDefaultInPath + "\\" + sRefDir + "\\" + sTargetSubDir);
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
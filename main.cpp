#include <iostream>
#include <iomanip>
#include <string>
#include <locale>
#include <vector>
#include <algorithm>

#include <chrono>

#include <filesystem>
#include <fstream>
#include <sstream>

#include <thread>
#include <mutex>

#include <sha256.h>

#include "json11/json11.hpp"
//Json parser lib: https://github.com/dropbox/json11

#ifndef JSON11_ENABLE_DR1467_CANARY
#define JSON11_ENABLE_DR1467_CANARY 0
#endif

namespace fs = std::filesystem;

#define VERSION "version 2.0"

//TODO: Load this data from .ini
std::string sDefaultInPath;
std::string sDefaultOutpPath;

std::string sRefDir;
std::string sTargetSubDir;
std::string sOutSubDir;

std::string sConfigPath = "config.json";

std::vector<std::string> inputLocals = { "RU", "CS", "DE", "FR", "N", "JP" };
std::vector<std::string> outputLocals = { "data_ru", "data_cs", "data_de", "data_fr", "data_n", "data_jp" };
std::vector<std::string> enLocVariants = {"data", "data_en"};

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

std::string InLower(const std::string &inHigher)
{
    std::string strcopy(inHigher);
    std::transform(strcopy.begin(), strcopy.end(), strcopy.begin(), [](unsigned char c){ return std::tolower(c); });
    return strcopy;
}

std::string strToWstr(const std::string& inStr)
{
    return std::string(inStr.begin(), inStr.end());
}

//std::string GetHexDigest(const std::string& inpString)
//{
//    return std::string("nop");
//}

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
/***************************************File work*******************************************/
/*******************************************************************************************/
node GetFileParams(const fs::path& entry)
{
    node newNode;
    std::ifstream fsCurFile;
    //wchar_t *readed = 0;

    newNode.fpath = entry;
    newNode.fname = entry.filename().string();
    newNode.dpath = newNode.fpath.parent_path();

    fsCurFile.open(newNode.fpath, std::ios_base::in | std::ios_base::binary);
    newNode.fhash = sha256(std::string((std::istreambuf_iterator<char>(fsCurFile)), std::istreambuf_iterator<char>()));
    fsCurFile.close();

    return newNode;
}

/*******************************************************************************************/
void CheckDiffAndCopy(const std::string& rHash, const std::string& sTarget, const std::string& inLoc, const std::string& outLoc)
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
            fs::create_directories(InLower(realFolderPath.string()));
            fs::copy(nTarget.fpath, InLower(realFolderPath.string()) + "\\" + InLower(nTarget.fname));
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
void ReadDirrectory(const fs::path& srSearchPath)
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
            if(bEnableProgressBar) filesPassed++;
            if(entry.path().extension().string() == ".flv" && bIgnoreFlv)
            {
                if(bEnableLogging) logFile << "Flv file ignored: " << filenameStr << std::endl;
            }
            else
            {
                node nReference = GetFileParams(entry.path());
                for(size_t i = 0; i < inputLocals.size(); i++)
                {
                    std::string sCurPath = entry.path().string();
                    std::string sForReplace = "\\" + inputLocals[i] + "\\";
                    std::string sSubstr = std::string("\\" + sRefDir + "\\");
                    sCurPath.replace(sCurPath.find(sSubstr), sSubstr.length(), sForReplace);
                    fs::path pCurPath = fs::path(sCurPath);
                    if(fs::exists(pCurPath))
                        vWorkerThreads.emplace_back(CheckDiffAndCopy, nReference.fhash, sCurPath, inputLocals.at(i), outputLocals.at(i));
                }
            }
            
        }
        else
            if(bEnableLogging) logFile << "Unknown file" << " [?]" << filenameStr << std::endl;
    }
}

/*******************************************************************************************/
void CheckInputPaths(std::vector<std::string>& inputsCheck, std::vector<std::string>& outputsCheck)
{
    for(size_t i = 0; i < inputsCheck.size(); i++)
    {
        fs::path curPath = fs::path(sDefaultInPath + "\\" + inputsCheck.at(i) + "\\" + sTargetSubDir);
        if(!fs::exists(curPath))
        {
            inputsCheck.erase(inputsCheck.begin() + i);
            outputsCheck.erase(outputsCheck.begin() + i);
        }
    }
}

/*******************************************************************************************/
bool ReadConfiguration()
{
    std::ifstream jsonConfig;
    #define Debug

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
                sDefaultInPath = strToWstr(json["default_input_path"].string_value());

            if(sDefaultOutpPath.empty())
                sDefaultOutpPath = strToWstr(json["default_output_path"].string_value());
            
            if(sRefDir.empty())
                sRefDir = strToWstr(json["reference_dirrectory"].string_value());
            
            if(sTargetSubDir.empty())
                sTargetSubDir = strToWstr(json["target_subdirrectory"].string_value());
                
            if(sOutSubDir.empty())
                sOutSubDir = strToWstr(json["output_subdirrectory"].string_value());

            inputLocals.clear();
            for(auto &it : json["input_locals"].array_items())
            {
                inputLocals.push_back(strToWstr(it.string_value()));
            }

            outputLocals.clear();
            for(auto &it : json["output_locals"].array_items())
            {
                outputLocals.push_back(strToWstr(it.string_value()));
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

void CopyRemove(const std::string& from, const std::string& to)
{
    if(fs::exists(to)) fs::remove(to);
    fs::copy(from, to);
}

void RecursiveCopy(const std::string& from, const std::string& to, const std::string& prefix)
{
    for (const auto& entry : fs::directory_iterator(from)) 
    {
        const auto filenameStr = entry.path().filename().string();
        if (entry.is_directory()) 
        {
            RecursiveCopy(entry.path().string(), to, prefix);
            if(bEnableProgressBar) DrawProgressBar(filesPassed, filesCount);
        }
        else if (entry.is_regular_file())
        {
            if(bEnableProgressBar) filesPassed++;
            if(entry.path().extension().string() == ".flv" && bIgnoreFlv)
            {
                if(bEnableLogging) logFile << "Flv file ignored: " << filenameStr << std::endl;
            }
            else
            {
                std::string sCurPath = entry.path().string();
                std::string sRealFolderPath = to + entry.path().parent_path().string().erase(0, prefix.size());
                std::string filepath = InLower(sRealFolderPath) + "\\" + InLower(entry.path().filename().string());
                //TODO: make it parallel
                fs::create_directories(InLower(sRealFolderPath));
                vWorkerThreads.emplace_back(CopyRemove, entry.path().string(), filepath);
            }
        }
        else
            if(bEnableLogging) logFile << "Unknown file" << " [?]" << filenameStr << std::endl;
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

/*******************************************************************************************/
size_t GetNumOfFilesInDirrectory(const fs::path& path)
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
        std::wcout << "Bad input or output dirrectory." << std::endl;

    //TODO: Delete report if already exists
    if(bEnableLogging) logFile.open("DiffAndCopyReport.txt", std::ios_base::out | std::ios_base::app);

    CheckInputPaths(inputLocals, outputLocals);

    std::cout << "Checking difference..." << std::endl;
    fs::path curPath = fs::path(sDefaultInPath + "\\" + sRefDir + "\\" + sTargetSubDir);

    if(!fs::exists(curPath))
    {
        std::cout << "Input path not found." << std::endl;
        return 0;
    }

    if(bEnableProgressBar) filesCount =  GetNumOfFilesInDirrectory(curPath);
    ReadDirrectory(curPath);

    JoinThreads();

    //TODO: Check is already exists
    std::cout << std::endl << "Copying EN folder." << std::endl;
    std::string referenceOut = sDefaultOutpPath + "\\" + sOutSubDir + "\\" + sTargetSubDir;
    filesPassed = 0;
    fs::create_directories(InLower(referenceOut));
    RecursiveCopy(curPath.string(), InLower(referenceOut), curPath.string());
    JoinThreads();

    if(bIgnoreFlv)
    { 
        std::string optres_path;
        std::string outres_path;
        std::cout << std::endl << "Copying optimized." << std::endl;
        for (size_t i = 0; i < outputLocals.size(); i++)
        {
            optres_path = sDefaultInPath + "\\" + "opt_res" + "\\" + outputLocals[i];
            outres_path = sDefaultOutpPath + "\\" + sOutSubDir + "\\" + outputLocals.at(i);
            if(fs::exists(optres_path))
            {
                filesPassed = 0;
                if(bEnableProgressBar) filesCount =  GetNumOfFilesInDirrectory(optres_path);

                fs::create_directories(InLower(outres_path));
                RecursiveCopy(optres_path, InLower(outres_path), optres_path);

                JoinThreads();
            }
        }

        //for en local
        for (size_t i = 0; i < enLocVariants.size(); i++)
        {
            optres_path = sDefaultInPath + "\\" + "opt_res" + "\\" + enLocVariants.at(i);
            outres_path = sDefaultOutpPath + "\\" + sOutSubDir + "\\" + sTargetSubDir;
            if(fs::exists(optres_path))
            {
                if(enLocVariants.at(i) == "data")
                {
                    outres_path = outres_path.erase(outres_path.size() - enLocVariants.at(i).size(), outres_path.size());
                }
                RecursiveCopy(optres_path, InLower(outres_path), optres_path);
                JoinThreads();
                break;
            }
        }

    }
    std::cout << "Operation time: " << StopTimer() << "s" << std::endl;
    if(bEnableLogging) logFile.close();

    return 0;
}
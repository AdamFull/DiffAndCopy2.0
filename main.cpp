#include <iostream>
#include <fstream>
#include <ctime>

#include <sha256.h>

#include "util.h"
#include "json11/json11.hpp"
//Json parser lib: https://github.com/dropbox/json11

#ifndef JSON11_ENABLE_DR1467_CANARY
#define JSON11_ENABLE_DR1467_CANARY 0
#endif

#define VERSION "version 2.0"

extern std::vector<std::thread> vWorkerThreads;

//TODO: Load this data from .ini
std::string sDefaultInPath;
std::string sDefaultOutpPath;

std::string sRefDir, sTargetSubDir, sOutSubDir, sOptRes;

std::string sConfigPath = "config.json";

std::vector<std::string> inputLocals;
std::vector<std::string> outputLocals;
std::vector<std::string> enLocVariants = {"data", "data_en"};

//Костыли из за "умных" людей, которым насрать на других
std::vector<std::string> english_variants;
std::vector<std::string> сzech_variants;
std::vector<std::string> german_variants;

std::vector<std::string> processed_locals;

bool bEnableLogging = true;
bool bEnableProgressBar = false;
bool bUseOptRes = false;

typedef struct {
    fs::path fpath;
    fs::path dpath;
    std::string fname;
    std::string fhash;
    size_t fSize;
} node;

extern std::ofstream logFile;

//Externals
extern size_t filesCount;
extern size_t filesPassed;
extern size_t diffCounter;
extern size_t errorCounter;

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
        
        std::string outPath = sDefaultOutpPath + SLASHES + sOutSubDir + SLASHES + outLoc + SLASHES + sTargetSubDir;
        std::string outPathPrefix = sDefaultInPath + SLASHES + inLoc + SLASHES + sTargetSubDir;
        std::string sRealFolderPath = outPath + nTarget.dpath.string().erase(0, outPathPrefix.size());
        fs::path realFolderPath = fs::path(sRealFolderPath);

        //mReadProtect.lock();
        try
        {
            fs::create_directories(InLower(realFolderPath.string()));
            CopyRemove(nTarget.fpath.string(), InLower(realFolderPath.string()) + SLASHES + InLower(nTarget.fname));
            diffCounter++;
        }
        catch(std::exception e)
        {
            if(bEnableLogging) logFile << "Error: " << e.what() << std::endl;
            errorCounter++;
        }
        //mReadProtect.unlock();
    }
}

/*******************************************************************************************/
void ReadDirrectory(const fs::path& srSearchPath)
{
    for (const auto& entry : fs::directory_iterator(srSearchPath)) 
    {
        if (entry.is_directory()) 
        {
            if(bEnableLogging) logFile << "INFO: " << "In folder " << entry.path().string() << " found " << diffCounter << " diffs" << std::endl;
            diffCounter = 0;
            if(bEnableProgressBar) DrawProgressBar(filesPassed, filesCount);
            if(vWorkerThreads.size() > 0) JoinThreads();
            ReadDirrectory(entry);
        }
        else if (entry.is_regular_file())
        {
            if(bEnableProgressBar) filesPassed++;
            if(entry.path().extension().string() == ".flv" && bUseOptRes)
            {
                if(bEnableLogging) logFile << "INFO: " << "Flv file ignored: " << entry.path() << std::endl;
            }
            else
            {
                node nReference = GetFileParams(entry.path());
                for(size_t i = 0; i < inputLocals.size(); i++)
                {
                    std::string sCurPath = entry.path().string();
                    std::string sForReplace = SLASHES + inputLocals[i] + SLASHES;
                    std::string sSubstr = std::string(SLASHES + sRefDir + SLASHES);
                    sCurPath.replace(sCurPath.find(sSubstr), sSubstr.length(), sForReplace);
                    fs::path pCurPath = fs::path(sCurPath);
                    if(fs::exists(pCurPath))
                        vWorkerThreads.emplace_back(CheckDiffAndCopy, nReference.fhash, sCurPath, inputLocals.at(i), outputLocals.at(i));
                }
                
            }
            
        }
        else
            if(bEnableLogging) logFile << "INFO: " << "Unknown file" << " [?]" << entry.path() << std::endl;
    }
}

/*******************************************************************************************/
void CheckInputPaths(std::vector<std::string>& inputsCheck, std::vector<std::string>& outputsCheck)
{
    for(size_t i = 0; i < inputsCheck.size(); i++)
    {
        fs::path curPath = fs::path(sDefaultInPath + SLASHES + inputsCheck.at(i) + SLASHES + sTargetSubDir);
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
    //#define Debug

    //#ifdef Debug
    //jsonConfig.open("..\\..\\config.json");
    //#else
    jsonConfig.open(sConfigPath);
    //#endif

    if(jsonConfig)
    {
        std::string jsonData = std::string((std::istreambuf_iterator<char>(jsonConfig)), std::istreambuf_iterator<char>());
        std::string err;
        const auto json = json11::Json::parse(jsonData, err);
        if(!json.is_null())
        {
            if(sRefDir.empty())
                sRefDir = json["reference_dirrectory"].string_value();
            
            if(sTargetSubDir.empty())
                sTargetSubDir = json["target_subdirrectory"].string_value();
                
            if(sOutSubDir.empty())
                sOutSubDir = json["output_subdirrectory"].string_value();

            sOptRes = json["optimized_resources_subdir"].string_value();

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

            for(auto &it : json["english_variants"].array_items())
            {
                english_variants.push_back(it.string_value());
            }
            for(auto &it : json["сzech_variants"].array_items())
            {
                сzech_variants.push_back(it.string_value());
            }
            for(auto &it : json["german_variants"].array_items())
            {
                german_variants.push_back(it.string_value());
            }

            bEnableLogging = json["enable_logging"].bool_value();
            bEnableProgressBar = json["enable_progressbar"].bool_value();
            bUseOptRes = json["use_optimized_resources"].bool_value();
            
            jsonConfig.close();
            return true;
        }
    }
    return false;
}

/*******************************************************************************************/


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
            return 0;
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

    if(bEnableLogging)
    {
        time_t rawtime;
        struct tm * timeinfo;

        std::string report = "log/DiffAndCopyReport_%s.txt";
        std::string report_time;
        std::string report_name;

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        report_time.resize(20);
        strftime(report_time.data(),report_time.size(),"%d-%m-%Y_%H.%M.%S",timeinfo);

        size_t report_size = report.size() + report_time.size() - 2;
        report_name.resize(report_size);
        sprintf(report_name.data(), report.c_str(), report_time.c_str());
        report_name.pop_back();
        if(!fs::exists("log/")) fs::create_directory("log/");
        logFile.open(report_name, std::fstream::out | std::fstream::binary);
    } 

    CheckInputPaths(inputLocals, outputLocals);

    std::cout << "Checking difference..." << std::endl;
    fs::path curPath = fs::path(sDefaultInPath + SLASHES + sRefDir + SLASHES + sTargetSubDir);

    if(!fs::exists(curPath))
    {
        std::cout << "Input path: " << curPath << " was not found." << std::endl;
        return 0;
    }

    if(bEnableProgressBar) filesCount =  GetNumOfFilesInDirrectory(curPath);
    ReadDirrectory(curPath);

    //TODO: Check is already exists
    std::cout << std::endl << "Copying EN folder." << std::endl;
    std::string referenceOut = sDefaultOutpPath + SLASHES + sOutSubDir + SLASHES + sTargetSubDir;
    filesPassed = 0;
    fs::create_directories(InLower(referenceOut));
    RecursiveCopy(curPath.string(), InLower(referenceOut), curPath.string());

    if(bUseOptRes)
    { 
        filesPassed = 0;
        std::string optres_path;
        std::string outres_path;
        std::cout << std::endl << "Copying optimized." << std::endl;

        optres_path = sDefaultInPath + SLASHES + sOptRes;
        if(bEnableProgressBar) filesCount =  GetNumOfFilesInDirrectory(optres_path);

        for (size_t i = 0; i < outputLocals.size(); i++)
        {
            if(std::find(processed_locals.begin(), processed_locals.end(), outputLocals.at(i)) != processed_locals.end()) continue;
            if(outputLocals.at(i) == "data_cz")
            {
                for(std::vector<std::string>::iterator it = сzech_variants.begin(); it != сzech_variants.end(); ++it)
                {
                    optres_path = sDefaultInPath + SLASHES + sOptRes + SLASHES + *it;
                    if(fs::exists(optres_path)) break;
                }
            }
            else if(outputLocals.at(i) == "data_gr")
            {
                for(std::vector<std::string>::iterator it = german_variants.begin(); it != german_variants.end(); ++it)
                {
                    optres_path = sDefaultInPath + SLASHES + sOptRes + SLASHES + *it;
                    if(fs::exists(optres_path)) break;
                }
            }
            else
            {
                optres_path = sDefaultInPath + SLASHES + sOptRes + SLASHES + outputLocals.at(i);
            }
            
            outres_path = sDefaultOutpPath + SLASHES + sOutSubDir + SLASHES + outputLocals.at(i);
            if(fs::exists(optres_path))
            {
                processed_locals.push_back(outputLocals.at(i));

                fs::create_directories(InLower(outres_path));
                RecursiveCopy(optres_path, InLower(outres_path), optres_path);
            }
            else
            {
                if(bEnableLogging) logFile << "WARNING: " << "Localization " << outputLocals.at(i) << " was not found." << std::endl;
            }
            
        }

        //for en local
        filesPassed = 0;
        for (size_t i = 0; i < enLocVariants.size(); i++)
        {
            optres_path = sDefaultInPath + SLASHES + sOptRes + SLASHES + enLocVariants.at(i);
            outres_path = sDefaultOutpPath + SLASHES + sOutSubDir;
            
            if(fs::exists(optres_path))
            {
                if(enLocVariants.at(i) == "data")
                {
                    outres_path = outres_path.erase(outres_path.size() - enLocVariants.at(i).size(), outres_path.size());
                }
                
                RecursiveCopy(optres_path, InLower(outres_path), optres_path);
                break;
            }
        }

    }
    std::cout << std::endl << "Operation time: " << StopTimerInNormalTime() << std::endl;
    if(errorCounter > 0) std::cout << "INFO: " << "Found " << errorCounter << " errors while running. Check log." << std::endl;
    if(bEnableLogging) logFile.close();

    JoinThreads();

    return 0;
}
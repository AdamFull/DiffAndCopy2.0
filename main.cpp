#include <iostream>
#include <fstream>

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

std::vector<std::string> inputLocals = { "RU", "CS", "DE", "FR", "N", "JP" };
std::vector<std::string> outputLocals = { "data_ru", "data_cs", "data_de", "data_fr", "data_n", "data_jp" };
std::vector<std::string> enLocVariants = {"data", "data_en"};

bool bEnableLogging = true;
bool bEnableProgressBar = false;
bool bIgnoreFlv = false;

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
            optres_path = sDefaultInPath + "\\" + sOptRes + "\\" + outputLocals[i];
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
            optres_path = sDefaultInPath + "\\" + sOptRes + "\\" + enLocVariants.at(i);
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
    std::cout << "Operation time: " << StopTimerInNormalTime() << std::endl;
    if(bEnableLogging) logFile.close();

    return 0;
}
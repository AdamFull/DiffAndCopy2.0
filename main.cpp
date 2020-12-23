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

namespace fs = std::filesystem;

#define VERSION "version 2.0"

std::string sourcesDir = "E:\\svn\\game";
std::string outputDir = "E:\\svn\\output";
bool bIgnoreFlv = false;

//TODO: Load this data from .ini
std::vector<std::string> inputLocals = { "RU", "CS", "DE", "FR", "NL", "JP" };
std::vector<std::string> outputLocals = { "data_ru", "data_cs", "data_de", "data_fr", "data_nl", "data_jp" };

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
        
        std::string outPath = outputDir + "\\res\\" + outLoc + "\\data";
        std::string outPathPrefix = sourcesDir + "\\" + inLoc + "\\data";
        std::string sRealFolderPath = outPath + nTarget.dpath.string().erase(0, outPathPrefix.size());
        fs::path realFolderPath = fs::path(sRealFolderPath);

        //mReadProtect.lock();
        fs::create_directories(realFolderPath);
        fs::copy(nTarget.fpath, realFolderPath);
        //mReadProtect.unlock();
    }
}

void ReadDirrectory(fs::path srSearchPath)
{
    for (const auto& entry : fs::directory_iterator(srSearchPath)) 
    {
        const auto filenameStr = entry.path().filename().string();
        if (entry.is_directory()) 
            ReadDirrectory(entry);
        else if (entry.is_regular_file())
        {
            node nReference = GetFileParams(entry.path());
            for(size_t i = 0; i < inputLocals.size(); i++)
            {
                std::string sCurPath = entry.path().string();
                std::string sForReplace = "\\" + inputLocals[i] + "\\";
                std::string sSubstr = std::string("\\EN\\");
                sCurPath.replace(sCurPath.find(sSubstr), sSubstr.length(), std::string("\\" + inputLocals[i] + "\\"));
                fs::path pCurPath = fs::path(sCurPath);
                if(fs::exists(pCurPath))
                    vWorkerThreads.emplace_back(CheckDiffAndCopy, nReference.fhash, sCurPath, inputLocals.at(i), outputLocals.at(i));
                //TODO: Add my progress bar
            }
        }
        else
            std::cout << "" << " [?]" << filenameStr << '\n';
    }
}

void CheckInputPaths(std::vector<std::string> * inputsCheck, std::vector<std::string> * outputsCheck)
{
    for(size_t i = 0; i < inputsCheck->size(); i++)
    {
        fs::path curPath = fs::path(sourcesDir + "\\" + inputsCheck->at(i) + "\\data");
        if(!fs::exists(curPath))
        {
            inputsCheck->erase(inputsCheck->begin() + i);
            outputsCheck->erase(outputsCheck->begin() + i);
        }
    }
}

int main(int argc, const char * argv[])
{
    //TODO: add reading arguments
    StartTimer();
    CheckInputPaths(&inputLocals, &outputLocals);

    std::cout << "Checking difference..." << std::endl;
    fs::path curPath = fs::path(sourcesDir + "\\" + "EN" + "\\data");
    ReadDirrectory(curPath);

    for(std::thread & th : vWorkerThreads)
    {
        if(th.joinable())
            th.join();
    }
    vWorkerThreads.clear();

    //TODO: Check is already exists
    std::cout << "Copying EN folder." << std::endl;
    StartTimer();
    std::string referenceOut = outputDir + "\\res\\data";
    fs::create_directories(referenceOut);
    fs::copy(curPath, referenceOut, std::filesystem::copy_options::recursive);
     std::cout << "Operation time: " << StopTimer() << "s" << std::endl;

    return 0;
}
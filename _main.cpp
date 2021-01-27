#include <iostream>
#include <fstream>
#include <map>

#include "json11/json11.hpp"

#ifndef JSON11_ENABLE_DR1467_CANARY
#define JSON11_ENABLE_DR1467_CANARY 0
#endif

typedef std::map<std::string, std::string> strmap_t;
typedef std::map<std::string, strmap_t> confmap_t;

confmap_t mVariants;

struct config_t
{
    std::string optimized_resources_subdir;
    std::string reference_dirrectory;
    std::string target_subdirrectory;
    std::string output_subdirrectory;

    std::vector<std::string> input_locals;
    std::vector<std::string> output_locals;
    std::vector<std::string> english_variants;
    std::vector<std::string> сzech_variants;
    std::vector<std::string> german_variants;

    bool enable_logging;
    bool enable_progressbar;

    std::string default_build;
};

config_t configuration;

int main()
{
    std::ifstream jsonConfig;
    jsonConfig.open("../../newconfig.json");

    if(jsonConfig)
    {
        std::string jsonData = std::string((std::istreambuf_iterator<char>(jsonConfig)), std::istreambuf_iterator<char>());
        std::string err;
        const auto json = json11::Json::parse(jsonData, err);

        if(!json.is_null())
        {
            //Config for all builds
            const auto defCfgJson = json11::Json::parse(json["default"].dump(), err);

            if(!defCfgJson.is_null())
            {
                configuration.optimized_resources_subdir = defCfgJson["optimized_resources_subdir"].string_value();
                configuration.reference_dirrectory = defCfgJson["reference_dirrectory"].string_value();
                configuration.target_subdirrectory = defCfgJson["target_subdirrectory"].string_value();
                configuration.output_subdirrectory = defCfgJson["output_subdirrectory"].string_value();

                for(auto &it : defCfgJson["input_locals"].array_items()) configuration.input_locals.push_back(it.string_value());
                for(auto &it : defCfgJson["output_locals"].array_items()) configuration.output_locals.push_back(it.string_value());
                for(auto &it : defCfgJson["english_variants"].array_items()) configuration.english_variants.push_back(it.string_value());
                for(auto &it : defCfgJson["сzech_variants"].array_items()) configuration.сzech_variants.push_back(it.string_value());
                for(auto &it : defCfgJson["german_variants"].array_items()) configuration.german_variants.push_back(it.string_value());
            }
        }

        const auto buildVariantsJson = json11::Json::parse(json["default"].dump(), err);
    }

    return 0;
}
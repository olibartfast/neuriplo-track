#pragma once
#include "AppConfig.hpp"
#include <opencv2/core/utility.hpp>

class CommandLineParser {
public:
    static AppConfig parseCommandLineArguments(int argc, char *argv[]);

private:
    static const std::string params;
    static void printHelpMessage(const cv::CommandLineParser& parser);
    static void validateArguments(const cv::CommandLineParser& parser);
    static std::set<int> mapClassesToIds(const std::vector<std::string>& classesToTrack, 
                                         const std::vector<std::string>& allClasses);
};

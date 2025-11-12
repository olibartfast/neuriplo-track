#include "MultiObjectTrackingApp.hpp"
#include "CommandLineParser.hpp"
#include <glog/logging.h>

int main(int argc, char *argv[]) {
    try {
        AppConfig config = CommandLineParser::parseCommandLineArguments(argc, argv);
        MultiObjectTrackingApp app(config);
        app.run();
        return 0;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 1;
    }
}

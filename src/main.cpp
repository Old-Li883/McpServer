#include <iostream>
#include "Config.h"

int main() {
    Config& config = Config::getInstance();

    std::cout << "=== Server Config ===" << std::endl;
    std::cout << "Port: " << config.getServerPort() << std::endl;
    std::cout << "Mode: " << config.getServerMode() << std::endl;

    std::cout << "\n=== Logging Config ===" << std::endl;
    std::cout << "Log File Path: " << config.getLogFilePath() << std::endl;
    std::cout << "Log Level: " << config.getLogLevel() << std::endl;
    std::cout << "Log File Size: " << config.getLogFileSize() << std::endl;
    std::cout << "Log File Count: " << config.getLogFileCount() << std::endl;
    std::cout << "Console Output: " << (config.getLogConsoleOutput() ? "true" : "false") << std::endl;

    return 0;
}

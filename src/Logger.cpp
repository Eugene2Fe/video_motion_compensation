#include "Logger.hpp"

Logger::Logger(const std::string& log_filename) {
    // logFile.open(log_filename, std::ios::app); // откр в режиме добавления
    logFile.open(log_filename, std::ios::out); // В режиме перезаписи
    if (!logFile) {
        std::cerr << "Error: cannot open file for logs!" << std::endl;
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::string logEntry = getTimeStamp() + " [" + levelToString(level) + "] " + message;

    if (logFile.is_open()) {
        logFile << logEntry << std::endl;
    }

    if (level != LogLevel::TO_FILE_ONLY) {
        std::cout << logEntry << std::endl;
    }
}

std::string Logger::getTimeStamp() {
    std::time_t now = std::time(nullptr);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buffer;
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::TO_FILE_ONLY: return "TO_FILE_ONLY";
    }
    return "UNKNOWN";
}

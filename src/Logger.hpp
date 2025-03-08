#ifndef LOGGER_H
#define LOGGER_H

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

enum class LogLevel { INFO, WARNING, ERROR, TO_FILE_ONLY };

class Logger {
   public:
    explicit Logger(const std::string& filename);
    ~Logger();

    void log(LogLevel level, const std::string& message);

    template <typename T, typename... Args>
    void log(LogLevel level, const T& first, const Args&... args) {
        std::ostringstream oss;
        oss << first;
        if constexpr (sizeof...(args) > 0) {
            (oss << ... << args);
        }
        log(level, oss.str());
    }

   private:
    std::ofstream logFile;
    std::string getTimeStamp();
    std::string levelToString(LogLevel level);
};

#endif  // LOGGER_H

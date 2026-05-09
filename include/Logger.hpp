#ifndef COPYPASTA_LOGGER_HPP
#define COPYPASTA_LOGGER_HPP

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>

#define LOG_LEVEL_DEBUG_FULL -1
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_NONE  3
#define LOG_LEVEL_ERROR 4

namespace copypasta {

// Set active level
extern int LOGGER_LEVEL;

std::string inline currentTime(){                                                    
    using namespace std::chrono;                                                
    auto now = system_clock::now();                                             
    auto secs = time_point_cast<std::chrono::seconds>(now);                     
    auto micros = duration_cast<std::chrono::microseconds>(now - secs).count(); 
    auto millis = micros / 1000;                                                
    auto micros_rem = micros % 1000;                                            
                                                                                
    std::time_t t = system_clock::to_time_t(secs);                              
    std::tm tm{};                                                               

#ifdef _WIN32 // fk this sht
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
                                                                                
    std::ostringstream oss;                                                     
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");                             
    oss << '.' << std::setw(3) << std::setfill('0') << millis;                  
    oss << std::setw(3) << std::setfill('0') << micros_rem;                     
    return oss.str();                                                          
}

// Core log macro
#define LOG(level, label, msg)                                        \
    do {                                                              \
        if ((level) >= LOGGER_LEVEL) {                                \
            std::ostream* out = &std::cout;                           \
            if((level) >= LOG_LEVEL_ERROR){                           \
              out = &std::cerr;                                       \
            }                                                         \
            (*out) << "[" << currentTime() << "] "                    \
                      << label                                        \
                      << " "                                          \
                      << msg                                          \
                      << "\n";                                        \
        }                                                             \
    } while (0)

#define DEBUG_FULL(msg) LOG(LOG_LEVEL_DEBUG_FULL, "[DEBUG_FULL]", msg)
#define DEBUG(msg)      LOG(LOG_LEVEL_DEBUG,           "[DEBUG]", msg)
#define INFO(msg)       LOG(LOG_LEVEL_INFO,            "[INFO]" , msg)
#define WARN(msg)       LOG(LOG_LEVEL_WARN,            "[WARN]" , msg)
#define LERROR(msg)      LOG(LOG_LEVEL_ERROR,          "[ERROR]", msg)

} // copypasta

#endif // _COPYPASTA_LOGGER

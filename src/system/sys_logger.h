#ifndef SYS_LOGGER_H
#define SYS_LOGGER_H

#include <Arduino.h>
#include "../config/config.h"

enum LogLevel : uint8_t {
    LOG_INFO = 0,
    LOG_WARN = 1,
    LOG_ERR  = 2
};

struct LogEntry {
    uint32_t timestampSec;
    LogLevel level;
    char message[SYSLOG_LINE_LEN];
};

class SysLogger {
public:
    void log(LogLevel level, const char* fmt, ...);
    size_t getCount() const;
    const LogEntry& getEntry(size_t index) const; // 0 = найстаріший, count-1 = найновіший
    void clear();

private:
    LogEntry _buffer[SYSLOG_MAX_ENTRIES];
    size_t _head  = 0;
    size_t _count = 0;
};

extern SysLogger sysLog;

// Зручні макроси для виклику з будь-якого модуля
#define LOG_I(...) sysLog.log(LOG_INFO, __VA_ARGS__)
#define LOG_W(...) sysLog.log(LOG_WARN, __VA_ARGS__)
#define LOG_E(...) sysLog.log(LOG_ERR,  __VA_ARGS__)

#endif // SYS_LOGGER_H
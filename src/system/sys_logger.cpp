#include "sys_logger.h"
#include <stdarg.h>
#include <stdio.h>

SysLogger sysLog;

void SysLogger::log(LogLevel level, const char* fmt, ...) {
    LogEntry& entry = _buffer[_head];
    entry.timestampSec = millis() / 1000;
    entry.level = level;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, SYSLOG_LINE_LEN, fmt, args);
    va_end(args);

    _head = (_head + 1) % SYSLOG_MAX_ENTRIES;
    if (_count < SYSLOG_MAX_ENTRIES) {
        _count++;
    }

    const char* tag = (level == LOG_ERR) ? "ERR" : (level == LOG_WARN ? "WRN" : "INF");
    Serial.printf("[%lu.%03lus][%s] %s\n",
                  millis() / 1000, millis() % 1000, tag, entry.message);
}

size_t SysLogger::getCount() const {
    return _count;
}

const LogEntry& SysLogger::getEntry(size_t index) const {
    size_t start = (_count < SYSLOG_MAX_ENTRIES) ? 0 : _head;
    size_t realIdx = (start + index) % SYSLOG_MAX_ENTRIES;
    return _buffer[realIdx];
}

void SysLogger::clear() {
    _head = 0;
    _count = 0;
}
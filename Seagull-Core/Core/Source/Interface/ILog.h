#pragma once

#include "Logging/Log.h"

namespace SG
{

	void failed_assert(const char* file, int line, const char* statement);
	void output_debug_string(const char* str, ...);
	void output_debug_string_v(const char* str, va_list args);

	void print_unicode(const char* str, uint32_t logLevel);

#define SG_LOG_RAW(log_level, ...) Logger::WriteRaw((log_level), false, __VA_ARGS__)
#define SG_LOG(log_level, ...)     Logger::Write((log_level), __FILE__, __LINE__, __VA_ARGS__)

#define SG_LOG_INFO(...)           Logger::Write(SG_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define SG_LOG_DEBUG(...)          Logger::Write(SG_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define SG_LOG_ERROR(...)          Logger::Write(SG_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define SG_LOG_WARNING(...)        Logger::Write(SG_LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define SG_LOG_CRITICAL(...)       Logger::Write(SG_LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)

#define SG_LOG_IF(log_level, condition, ...) ((condition) ? Logger::Write((log_level), __FILE__, __LINE__, __VA_ARGS__) : (void)0)

}

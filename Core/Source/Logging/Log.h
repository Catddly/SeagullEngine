#pragma once

#include "Core/CompilerConfig.h"

#include <include/EASTL/vector.h>
#include <include/EASTL/string.h>

#include "BasicType/ISingleton.h"

#include "Interface/IFileSystem.h"
#include "Interface/IThread.h"


#ifndef SG_FILENAME_NAME_LENGTH_LOG
#define SG_FILENAME_NAME_LENGTH_LOG 23
#endif

namespace SG
{

	enum LogLevel
	{
		SG_LOG_LEVEL_NONE = 0x00,
		SG_LOG_LEVEL_RAW = 0x01,
		SG_LOG_LEVEL_DEBUG = 0x02,
		SG_LOG_LEVEL_INFO = 0x04,
		SG_LOG_LEVEL_WARNING = 0x08,
		SG_LOG_LEVEL_ERROR = 0x10,
		SG_LOG_LEVEL_CRITICAL = 0x20,
		SG_LOG_LEVEL_ALL = ~0
	};

	typedef void(*log_callback_t)(void* pUserData, const char* msg);
	typedef void(*log_close_t)(void* pUserData);
	typedef void(*log_flush_t)(void* pUserData);

	class Logger : public ISingleton
	{
	public:
		struct LogInfo
		{
			LogInfo(uint32_t logLevel, const char* file, int line, const char* format, ...);
			~LogInfo();

			eastl::string mMsg;
			const char* mFile;
			int mLine;
			uint32_t mLogLevel;
		};

		Logger(const char* appName, LogLevel level = SG_LOG_LEVEL_ALL);
		~Logger();

		static void OnInit(const char* appName, LogLevel level = SG_LOG_LEVEL_ALL);
		static void OnExit();

		static void SetLevel(LogLevel level);
		static void SetQuiet(bool bQuiet);
		static void SetTimeStamp(bool bEnable);
		static void SetRecordingFile(bool bEnable);
		static void SetRecordingThreadName(bool bEnable);
		static void SetConsoleLogging(bool bEnable);

		static uint32_t        GetLevel();
		static eastl::string   GetLastMessage();
		static bool            IsQuiet();
		static bool            IsRecordingTimeStamp();
		static bool            IsRecordingFile();
		static bool            IsRecordingThreadName();

		static void AddFile(const char* filename, FileMode fileMode, LogLevel logLevel);
		static void AddCallback(const char* id, uint32_t logLevel, void* userData, log_callback_t callback, log_close_t close = nullptr, log_flush_t flush = nullptr);

		static void Write(uint32_t level, const char* filename, int line_number, const char* message, ...);
		static void WriteRaw(uint32_t level, bool error, const char* message, ...);
	private:
		static void AddInitialLogFile(const char* appName);
		static uint32_t WritePreamble(char* buffer, uint32_t buffer_size, const char* file, int line);
		static bool CallbackExists(const char* id);

		struct LogCallback
		{
			LogCallback(const eastl::string& id, void* pUserData,
				log_callback_t callback,
				log_close_t close,
				log_flush_t flush,
				uint32_t level)
				: mID(id), mUserData(pUserData), mCallbackFunc(callback),
				mFlushFunc(flush), mCloseFunc(close), mLogLevel(level)
			{}

			eastl::string mID;
			void* mUserData;
			log_callback_t mCallbackFunc = nullptr;
			log_flush_t    mFlushFunc = nullptr;
			log_close_t    mCloseFunc = nullptr;
			uint32_t mLogLevel;
		};

		eastl::vector<LogCallback> mCallbacks;
		uint32_t mLogLevel;
		uint32_t mIndentation; // the count of infomations
		Mutex    mMutex;

		bool     mQuietMode;
		bool     mRecordTimestamp;
		bool     mRecordFile;
		bool     mRecordThreadName;

		enum { SG_MAX_BUFFER = 1024 };

		static thread_local char sThreadLocalBuffer[SG_MAX_BUFFER + 2];
		static bool sConsoleLogging;
	};

}

// eastl::string ToString(const char* formatString, ...);
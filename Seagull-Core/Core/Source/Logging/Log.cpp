#include "Log.h"

#include "Interface/ILog.h"
#include "Interface/IMemory.h"

#include <time.h>

namespace SG
{

#define SG_LOG_PREAMBLE_SIZE (56 + SG_MAX_THREAD_NAME_LENGTH + SG_FILENAME_NAME_LENGTH_LOG)
#define SG_LOG_LEVEL_SIZE 7
#define SG_LOG_MESSAGE_OFFSET (SG_LOG_PREAMBLE_SIZE + SG_LOG_LEVEL_SIZE)

#ifndef SG_INDENTATION_SIZE_LOG
#define SG_INDENTATION_SIZE_LOG 4
#endif

	thread_local char Logger::sThreadLocalBuffer[SG_MAX_BUFFER + 2];
	bool Logger::sConsoleLogging = true;
	static Logger* sLogger = nullptr;
	Mutex Logger::sLogMutex;

	const char* get_filename_from_path(const char* path)
	{
		for (auto ptr = path; *ptr; ++ptr) {
			if (*ptr == '/' || *ptr == '\\') {
				path = ptr + 1;
			}
		}
		return path;
	}

	// default callback functions
	void log_write_default(void* user_data, const char* message)
	{
		auto* fs = (FileStream*)user_data;
		ASSERT(fs);
		sgfs_write_to_stream(fs, message, strlen(message));
		sgfs_flush_stream(fs);
	}

	void log_close_default(void* user_data)
	{
		auto* fs = (FileStream*)user_data;
		ASSERT(fs);
		sgfs_close_stream(fs);
		sg_free(fs);
	}

	void log_flush_default(void* user_data)
	{
		auto* fs = (FileStream*)user_data;
		ASSERT(fs);
		sgfs_flush_stream(fs);
	}

	eastl::string get_timestamp()
	{
		time_t systemTime;
		time(&systemTime);
		eastl::string dateTime = ctime(&systemTime);
		eastl::replace(dateTime.begin(), dateTime.end(), '\n', ' ');
		return dateTime;
	}

	Logger::LogInfo::LogInfo(uint32_t logLevel, const char* file, int line, const char* format, ...)
		:mFile(file), mLine(line), mLogLevel(logLevel)
	{
		const unsigned BUFFER_SIZE = 4096;
		char           buf[BUFFER_SIZE];
		va_list arglist; // multiple args
		va_start(arglist, format);
		vsprintf_s(buf, BUFFER_SIZE, format, arglist /* here to expand */);
		va_end(arglist);
		mMsg = buf;

		// write to log and update indentation
		Logger::Write(mLogLevel, mFile, mLine, "{ %s", buf); // write the prefix
		{
			MutexLock lock{ sLogger->mMutex };
			++sLogger->mIndentation;
		}
	}

	Logger::LogInfo::~LogInfo()
	{
		{
			MutexLock lck{ sLogger->mMutex };
			--sLogger->mIndentation;
		}
		Logger::Write(mLogLevel, mFile, mLine, "} %s", mMsg.c_str()); // write all the message
	}

	Logger::Logger(const char* appName, LogLevel level)
		: mLogLevel((uint32_t)level)
		, mIndentation(0)
		, mQuietMode(false)
		, mRecordTimestamp(true)
		, mRecordFile(false)
		, mRecordThreadName(true)
	{
		Thread::set_main_thread();
		// the logger thread should be the main thread,
		// in case the logger won't be blocked out by other events
		Thread::set_curr_thread_name("MainThread");
	}

	Logger::~Logger()
	{
		// execute all the close event deferred
		for (LogCallback& callback : mCallbacks)
		{
			if (callback.mCloseFunc)
				callback.mCloseFunc(callback.mUserData);
		}
		mCallbacks.clear();
	}

	void Logger::OnInit(const char* appName, LogLevel level /*= LogLevel::SG_LOG_LEVEL_ALL*/)
	{
		if (!sLogger)
		{
			sLogger = sg_new(Logger, appName, level);
			sLogger->mMutex.Init();
			sLogMutex.Init();
			sLogger->AddInitialLogFile(appName);
		}
	}

	void Logger::OnExit()
	{
		sLogger->mMutex.Destroy();
		sLogMutex.Destroy();
		sg_delete(sLogger);
		sLogger = nullptr;
	}

	void Logger::SetLevel(LogLevel level) { sLogger->mLogLevel = level; }
	void Logger::SetQuiet(bool bQuiet) { sLogger->mQuietMode = bQuiet; }
	void Logger::SetTimeStamp(bool bEnable) { sLogger->mRecordTimestamp = bEnable; }
	void Logger::SetRecordingFile(bool bEnable) { sLogger->mRecordFile = bEnable; }
	void Logger::SetRecordingThreadName(bool bEnable) { sLogger->mRecordThreadName = bEnable; }
	void Logger::SetConsoleLogging(bool bEnable) { sLogger->sConsoleLogging = bEnable; }

	uint32_t Logger::GetLevel() { return sLogger->mLogLevel; }
	bool Logger::IsQuiet() { return sLogger->mQuietMode; }
	bool Logger::IsRecordingTimeStamp() { return sLogger->mRecordTimestamp; }
	bool Logger::IsRecordingFile() { return sLogger->mRecordFile; }
	bool Logger::IsRecordingThreadName() { return sLogger->mRecordThreadName; }

	eastl::string Logger::GetLastMessage()
	{
		return "";
	}

	void Logger::AddFile(const char* filename, FileMode fileMode, LogLevel logLevel)
	{
		if (filename == NULL)
			return;

		FileStream fs{};
		if (sgfs_open_stream_from_path(SG_RD_LOG, filename, fileMode, &fs))
		{
			auto* user = (FileStream*)sg_malloc(sizeof(FileStream));
			*user = fs;

			// add callback functions will try to acquire mutex
			char path[SG_MAX_FILEPATH] = { 0 };
			sgfs_append_path_component(sgfs_get_resource_directory(SG_RD_LOG), filename, path);
			AddCallback(path, logLevel, user, log_write_default, log_close_default, log_flush_default);

			{
				MutexLock lck{ sLogger->mMutex };

				eastl::string header;
				if (sLogger->mRecordTimestamp)
					header += "date           time       ";
				if (sLogger->mRecordThreadName)
					header += "[thread name/id ]";
				if (sLogger->mRecordFile)
					header += "                   file:line  ";
				header += "     v |\n";
				sgfs_write_to_stream(&fs, header.c_str(), header.size());
				sgfs_flush_stream(&fs);
			}

			Write(SG_LOG_LEVEL_INFO, __FILE__, __LINE__, "Opened log file %s", filename);
		}
		else
		{
			Write(SG_LOG_LEVEL_ERROR, __FILE__, __LINE__, "Failed to create log file %s", filename);
		}
	}

	void Logger::AddCallback(const char* id, uint32_t logLevel, void* userData, log_callback_t callback, log_close_t close /*= nullptr*/, log_flush_t flush /*= nullptr*/)
	{
		MutexLock lck{ sLogger->mMutex };
		if (!CallbackExists(id))
		{
			sLogger->mCallbacks.emplace_back(id, userData, callback, close, flush, logLevel);
		}
		else
		{
			// close current file stream
			close(userData);
		}
	}

	void Logger::Write(uint32_t level, const char* filename, int lineNumber, const char* message, ...)
	{
		static eastl::pair<uint32_t, const char*> logLevelPrefixes[] =
		{
			eastl::pair<uint32_t, const char*>{ LogLevel::SG_LOG_LEVEL_WARNING, " WARN| " },
			eastl::pair<uint32_t, const char*>{ LogLevel::SG_LOG_LEVEL_INFO,    " INFO| " },
			eastl::pair<uint32_t, const char*>{ LogLevel::SG_LOG_LEVEL_DEBUG,   " DEBUG| " },
			eastl::pair<uint32_t, const char*>{ LogLevel::SG_LOG_LEVEL_ERROR,   " ERROR| " },
			eastl::pair<uint32_t, const char*>{ LogLevel::SG_LOG_LEVEL_CRITICAL," CRIT| " }
		};

		uint32_t logLevels[SG_LOG_LEVEL_SIZE];
		uint32_t logLevelCount = 0;

		// check flags
		for (uint32_t i = 0; i < sizeof(logLevelPrefixes) / sizeof(logLevelPrefixes[0]); ++i)
		{
			eastl::pair<uint32_t, const char*>& it = logLevelPrefixes[i];
			if (it.first & level)
			{
				logLevels[logLevelCount] = i;
				++logLevelCount;
			}
		}

		uint32_t preableEnd = WritePreamble(sThreadLocalBuffer, SG_LOG_PREAMBLE_SIZE, filename, lineNumber);

		// prepare indentation
		uint32_t indentation = sLogger->mIndentation * SG_INDENTATION_SIZE_LOG;
		memset(sThreadLocalBuffer + preableEnd, ' ', indentation);

		uint32_t offset = preableEnd + SG_LOG_LEVEL_SIZE + indentation;
		va_list args;
		va_start(args, message);
		offset += vsnprintf(sThreadLocalBuffer + offset, SG_MAX_BUFFER - offset, message, args);
		va_end(args);

		offset = (offset > SG_MAX_BUFFER) ? SG_MAX_BUFFER : offset;
		sThreadLocalBuffer[offset] = '\n';
		sThreadLocalBuffer[offset + 1] = 0;

		// Log for each flag
		for (uint32_t i = 0; i < logLevelCount; ++i)
		{
			strncpy(sThreadLocalBuffer + preableEnd, logLevelPrefixes[logLevels[i]].second, SG_LOG_LEVEL_SIZE);

			if (sConsoleLogging)
			{
				if (sLogger->mQuietMode)
				{
					MutexLock lck(sLogMutex);
					if (level & SG_LOG_LEVEL_ERROR)
						print_unicode(sThreadLocalBuffer, level);
				}
				else
				{
					MutexLock lck(sLogMutex);
					print_unicode(sThreadLocalBuffer, level);
				}
			}

			MutexLock lock{ sLogger->mMutex };
			for (LogCallback& callback : sLogger->mCallbacks) // for all the callbacks in the callback stack, call it!
			{
				if (callback.mLogLevel & logLevels[i])
					callback.mCallbackFunc(callback.mUserData, sThreadLocalBuffer);
			}
		}
	}

	void Logger::WriteRaw(uint32_t level, bool error, const char* message, ...)
	{
		va_list args;
		va_start(args, message);
		vsnprintf(sThreadLocalBuffer, SG_MAX_BUFFER, message, args);
		va_end(args);

		if (sConsoleLogging)
		{
			if (sLogger->mQuietMode)
			{
				MutexLock lck(sLogMutex);
				if (error)
					print_unicode(sThreadLocalBuffer, level);
			}
			else
			{
				MutexLock lck(sLogMutex);
				print_unicode(sThreadLocalBuffer, level);
			}
		}

		MutexLock lock{ sLogger->mMutex };
		for (LogCallback& callback : sLogger->mCallbacks)
		{
			if (callback.mLogLevel & level)
				callback.mCallbackFunc(callback.mUserData, sThreadLocalBuffer);
		}
	}

	void Logger::AddInitialLogFile(const char* appName)
	{
		const char* extension = ".log";
		const size_t extensionLength = strlen(extension);

		char exeFileName[SG_MAX_FILEPATH] = { 0 };
		strcpy(exeFileName, appName);

		// minimum length check
		if (exeFileName[0] == 0 || exeFileName[1] == 0)
		{
			strncpy(exeFileName, "Log", 3);
		}
		strncat(exeFileName, extension, extensionLength);

		AddFile(exeFileName, SG_FM_WRITE_BINARY_ALLOW_READ, SG_LOG_LEVEL_ALL);
	}

	uint32_t Logger::WritePreamble(char* buffer, uint32_t bufferSize, const char* file, int line)
	{
		uint32_t pos = 0;
		// Date and time
		if (sLogger->mRecordTimestamp && pos < bufferSize)
		{
			time_t  t = time(NULL);
			tm time_info;
#if defined(SG_PLATFORM_WINDOWS) || defined(XBOX)
			localtime_s(&time_info, &t);
#else
			localtime_r(&t, &time_info);
#endif
			pos += snprintf(buffer + pos, bufferSize - pos, "%04d-%02d-%02d %02d:%02d:%02d ",
				1900 + time_info.tm_year, 1 + time_info.tm_mon, time_info.tm_mday,
				time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
		}

		if (sLogger->mRecordThreadName && pos < bufferSize)
		{
			char thread_name[SG_MAX_THREAD_NAME_LENGTH + 1] = { 0 };
			Thread::get_curr_thread_name(thread_name, SG_MAX_THREAD_NAME_LENGTH + 1);
			pos += snprintf(buffer + pos, bufferSize - pos, "[%-15s]", thread_name[0] == 0 ? "NoName" : thread_name);
		}

		// file and line
		if (sLogger->mRecordFile && pos < bufferSize)
		{
			file = get_filename_from_path(file);
			pos += snprintf(buffer + pos, bufferSize - pos, " %22.*s:%-5u ", SG_FILENAME_NAME_LENGTH_LOG, file, line);
		}

		return pos;
	}

	bool Logger::CallbackExists(const char* id)
	{
		for (const LogCallback& callback : sLogger->mCallbacks)
		{
			if (callback.mID == id)
				return true;
		}
		return false;
	}

}
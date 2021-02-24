#pragma once

#include "IOperatingSystem.h"

#include <cstring>

#define SG_MAX_FILEPATH 256

namespace SG
{

#ifdef __cplusplus
	extern "C"
	{
#endif

		/// 资源装载用途
		typedef enum ResourceMount
		{
			SG_RM_CONTENT = 0, /// Installed game directory / bundle resource directory
			SG_RM_DEBUG,       /// For storing debug data such as log files. To be used only during development
			SG_RM_SAVE_0,      /// Save game data mount 0
			SG_RM_COUNT
		} ResourceMount;

		typedef enum ResourceDirectory
		{
			SG_RD_SHADER_BINARIES = 0, /// shader binaries
			SG_RD_SHADER_SOURCES,     /// shader source
			SG_RD_PIPELINE_CACHE,
			SG_RD_TEXTURES,
			SG_RD_MESHES,
			SG_RD_FONTS,
			SG_RD_ANIMATIONS,
			SG_RD_GPU_CONFIG,
			SG_RD_LOG,
			SG_RD_SCRIPTS,
			SG_RD_OHTER_FILES,

			// libraries can have their own directories
			____sg_rd_lib_counter_begin = SG_RD_OHTER_FILES + 1,

			// Add libraries here
			SG_RD_MIDDLEWARE_0 = ____sg_rd_lib_counter_begin,
			SG_RD_MIDDLEWARE_1,
			SG_RD_MIDDLEWARE_2,
			SG_RD_MIDDLEWARE_3,
			SG_RD_MIDDLEWARE_4,
			SG_RD_MIDDLEWARE_5,
			SG_RD_MIDDLEWARE_6,
			SG_RD_MIDDLEWARE_7,
			SG_RD_MIDDLEWARE_8,
			SG_RD_MIDDLEWARE_9,
			SG_RD_MIDDLEWARE_10,
			SG_RD_MIDDLEWARE_11,
			SG_RD_MIDDLEWARE_12,
			SG_RD_MIDDLEWARE_13,
			SG_RD_MIDDLEWARE_14,
			SG_RD_MIDDLEWARE_15,

			____sg_rd_lib_counter_end = ____sg_rd_lib_counter_begin + 99 * 2,
			SG_RD_COUNT
		} ResourceDirectory;

		typedef enum SeekBaseOffset
		{
			SG_SBO_START_OF_FILE = 0,
			SG_SBO_CURRENT_POSITION,
			SG_SBO_END_OF_FILE
		} SeekBaseOffset;

		typedef enum FileMode
		{
			SG_FM_READ = 1 << 0,
			SG_FM_WRITE = 1 << 1,
			SG_FM_APPEND = 1 << 2,
			SG_FM_BINARY = 1 << 3,
			SG_FM_ALLOW_READ = 1 << 4, // Read access to other processes, useful for log system

			SG_FM_READ_WRITE = SG_FM_READ | SG_FM_WRITE,
			SG_FM_READ_APPEND = SG_FM_READ | SG_FM_APPEND,
			SG_FM_WRITE_BINARY = SG_FM_WRITE | SG_FM_BINARY,
			SG_FM_READ_BINARY = SG_FM_READ | SG_FM_BINARY,
			SG_FM_APPEND_BINARY = SG_FM_APPEND | SG_FM_BINARY,
			SG_FM_READ_WRITE_BINARY = SG_FM_READ | SG_FM_WRITE | SG_FM_BINARY,
			SG_FM_READ_APPEND_BINARY = SG_FM_READ | SG_FM_APPEND | SG_FM_BINARY,
			SG_FM_WRITE_ALLOW_READ = SG_FM_WRITE | SG_FM_ALLOW_READ,
			SG_FM_APPEND_ALLOW_READ = SG_FM_READ | SG_FM_ALLOW_READ,
			SG_FM_READ_WRITE_ALLOW_READ = SG_FM_READ | SG_FM_WRITE | SG_FM_ALLOW_READ,
			SG_FM_READ_APPEND_ALLOW_READ = SG_FM_READ | SG_FM_APPEND | SG_FM_ALLOW_READ,
			SG_FM_WRITE_BINARY_ALLOW_READ = SG_FM_WRITE | SG_FM_BINARY | SG_FM_ALLOW_READ,
			SG_FM_APPEND_BINARY_ALLOW_READ = SG_FM_APPEND | SG_FM_BINARY | SG_FM_ALLOW_READ,
			SG_FM_READ_WRITE_BINARY_ALLOW_READ = SG_FM_READ | SG_FM_WRITE | SG_FM_BINARY | SG_FM_ALLOW_READ,
			SG_FM_READ_APPEND_BINARY_ALLOW_READ = SG_FM_READ | SG_FM_APPEND | SG_FM_BINARY | SG_FM_ALLOW_READ
		} FileMode;

		typedef struct IFileSystem IFileSystem;

		typedef struct MemoryStream
		{
			uint8_t* pBuffer;
			size_t   cursor;
			bool     owner;
		} MemoryStream;

		typedef struct FileStream
		{
			IFileSystem* pIO;
			union
			{
				FILE* file;
				MemoryStream memory;
				void* pUser;
			};
			ssize_t   size;
			FileMode  mode;
		} FileStream;

		typedef struct FileSystemInitDescription
		{
			const char* appName;
			void* pPlatformData = nullptr;
			const char* resourceMounts[SG_RM_COUNT] = {};
		} FileSystemInitDescription;

		typedef struct IFileSystem
		{
			bool        (*open)(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);
			bool        (*close)(FileStream* pFile);
			size_t		(*read)(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes);
			size_t		(*write)(FileStream* pFile, const void* sourceBuffer, size_t byteCount);
			bool        (*seek)(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset);
			ssize_t		(*get_seek_position)(const FileStream* pFile);
			ssize_t		(*get_file_size)(const FileStream* pFile);
			bool        (*flush)(FileStream* pFile);
			bool        (*is_at_end)(const FileStream* pFile);
			const char* (*get_resource_mount)(ResourceMount mount);

			void* pUser;
		} IFileSystem;

		/// Default file system using C File IO or Bundled File IO (Android) based on the ResourceDirectory
		extern IFileSystem* pSystemFileIO;

		bool sgfs_init_file_system(FileSystemInitDescription* pDesc);
		void sgfs_exit_file_system(); // free the resources related to the file_system_api

		bool sgfs_open_stream_from_path(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);
		bool sgfs_open_stream_from_memory(const void* pBuffer, size_t bufferSize, FileMode mode, bool owner, FileStream* pOut);

		bool sgfs_close_stream(FileStream* stream);

		/// return the byte size of the stream
		size_t sgfs_read_from_stream(FileStream* pStream, void* pOutputBuffer, size_t bufferSizeInbytes);
		size_t sgfs_write_to_stream(FileStream* pStream, const void* pSourceBuffer, size_t byteCount);

		/// offset to the dst in file stream
		bool sgfs_seek_stream(FileStream* pStream, SeekBaseOffset baseOffset, ssize_t byteOffset);
		ssize_t sgfs_get_offset_stream_position(const FileStream* pStream);

		/// get the size of the file. return -1 if the size if unknown
		ssize_t sgfs_get_stream_file_size(const FileStream* pStream);

		bool sgfs_flush_stream(FileStream* pStream);

		bool sgfs_is_stream_at_end(const FileStream* pStream);

		/// appends `pathComponent` to `basePath`, where `basePath` is assumed to be a directory.
		void sgfs_append_path_component(const char* basePath, const char* pathComponent, char* output);
		/// appends `newExtension` to `basePath`.
		/// if `basePath` already has an extension, `newExtension` will be appended to the end.
		void sgfs_append_path_extension(const char* basePath, const char* newExtension, char* output);

		/// Get `path`'s parent path, excluding the end seperator. 
		void sgfs_get_parent_path(const char* path, char* output);

		/// Get `path`'s file name, without extension or parent path.
		void sgfs_get_path_file_name(const char* path, char* output);

		/// Returns `path`'s extension, excluding the '.'.
		void sgfs_get_path_extension(const char* path, char* output);

		/// returns location set for resource directory in sgfs_set_path_for_resource_dir.
		const char* sgfs_get_resource_directory(ResourceDirectory resourceDir);
		void sgfs_set_path_for_resource_dir(IFileSystem* pIO, ResourceMount mount, ResourceDirectory resourceDir, const char* bundledFolder);

		static inline FileMode sgfs_file_mode_from_string(const char* modeName)
		{
			if (std::strcmp(modeName, "r") == 0)
				return SG_FM_READ;

			if (std::strcmp(modeName, "w") == 0)
				return SG_FM_WRITE;

			if (std::strcmp(modeName, "a") == 0)
				return SG_FM_APPEND;

			if (std::strcmp(modeName, "rb") == 0)
				return SG_FM_READ_BINARY;

			if (std::strcmp(modeName, "wb") == 0)
				return SG_FM_WRITE_BINARY;

			if (std::strcmp(modeName, "ab") == 0)
				return SG_FM_APPEND_BINARY;

			if (std::strcmp(modeName, "rw") == 0)
				return SG_FM_READ_WRITE;

			if (std::strcmp(modeName, "ra") == 0)
				return SG_FM_READ_APPEND;

			if (std::strcmp(modeName, "rwb") == 0)
				return SG_FM_READ_WRITE_BINARY;

			if (std::strcmp(modeName, "rab") == 0)
				return SG_FM_READ_APPEND_BINARY;

			return (FileMode)0;
		}

		static inline SG_CONSTEXPR const char* sgfs_file_mode_to_string(FileMode mode)
		{
			mode = (FileMode)(mode & ~SG_FM_ALLOW_READ);
			switch (mode)
			{
			case SG_FM_READ:		       return "r";
			case SG_FM_WRITE:			   return "w";
			case SG_FM_APPEND:			   return "a";
			case SG_FM_READ_BINARY:		   return "rb";
			case SG_FM_WRITE_BINARY:       return "wb";
			case SG_FM_APPEND_BINARY:	   return "ab";
			case SG_FM_READ_WRITE:		   return "rw";
			case SG_FM_READ_APPEND:        return "ra";
			case SG_FM_READ_WRITE_BINARY:  return "rwb";
			case SG_FM_READ_APPEND_BINARY: return "rab";
			default:                       return "r";
			}
		}

		/// Gets the time of last modification for the file at `fileName`, within 'resourceDir'.
		time_t sgfs_get_last_modified_time(ResourceDirectory resourceDir, const char* fileName);

#ifdef __cplusplus
	}
#endif

}
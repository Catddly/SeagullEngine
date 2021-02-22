#include "Interface/IFileSystem.h"
#include "Interface/ILog.h"
#include "Interface/IMemory.h"

#include <include/EASTL/utility.h>

namespace SG
{

	bool platform_open_file(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);
	bool platform_close_file(FileStream* pFile);

	/// every directory has its own IO
	typedef struct ResourceDirectoryInfo
	{
		IFileSystem* pIO;
		char         path[SG_MAX_FILEPATH] = {};
		bool         isBundled;
	} ResourceDirectoryInfo;

	/// all the directories
	static ResourceDirectoryInfo gResourceDirectories[SG_RD_COUNT] = {};

	static inline SG_CONSTEXPR const char sgfs_get_directory_separator()
	{
#if defined(SG_PLATFORM_WINDOWS)
		return '\\';
#else
		return '/';
#endif
	}

	/////////////////////////////////////////////////////////////////////////////
	/// Memory Stream Functions 
	/////////////////////////////////////////////////////////////////////////////

	static inline SG_CONSTEXPR size_t available_capacity(FileStream* pFile, size_t requestedCapacity)
	{
		return eastl::min((ssize_t)requestedCapacity,
			eastl::max((ssize_t)pFile->size - (ssize_t)pFile->memory.cursor, (ssize_t)0));
	}

	static bool memory_stream_close(FileStream* pFile)
	{
		MemoryStream* memStream = &pFile->memory;
		if (memStream->owner)
		{
			sg_free(memStream->pBuffer);
		}
		return true;
	}

	static size_t memory_stream_read(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes)
	{
		if (!(pFile->mode & SG_FM_READ))
		{
			SG_LOG_WARNING("Attempting to read to write-only buffer");
			return 0;
		}

		size_t byteToRead = available_capacity(pFile, bufferSizeInBytes);
		memcpy(outputBuffer, pFile->memory.pBuffer + pFile->memory.cursor, byteToRead);
		pFile->memory.cursor += byteToRead; // offset to the new pos
		return byteToRead; // how many bytes we had read
	}

	static size_t memory_stream_write(FileStream* pFile, const void* sourceBuffer, size_t byteCount)
	{
		if (!(pFile->mode & SG_FM_WRITE))
		{
			SG_LOG_WARNING("Attempting to write to read-only buffer");
			return 0;
		}

		size_t byteToWrite = available_capacity(pFile, byteCount);
		memcpy(pFile->memory.pBuffer + pFile->memory.cursor, sourceBuffer, byteToWrite);
		pFile->memory.cursor += byteToWrite; // offset to the new pos
		return byteToWrite; // how many bytes we had read
	}

	static bool memory_stream_seek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
	{
		switch (baseOffset)
		{
		case SG_SBO_START_OF_FILE:
		{
			if (seekOffset < 0 || seekOffset >= pFile->size)
			{
				SG_LOG_ERROR("seeking exceed the file boundary!");
				return false;
			}
			pFile->memory.cursor = seekOffset;
		}
		break;
		case SG_SBO_CURRENT_POSITION:
		{
			ssize_t newPosition = (ssize_t)pFile->memory.cursor + seekOffset;
			if (newPosition < 0 || newPosition >= pFile->size)
			{
				SG_LOG_ERROR("seeking exceed the file boundary!");
				return false;
			}
			pFile->memory.cursor = (size_t)newPosition;
		}
		break;

		case SG_SBO_END_OF_FILE:
		{
			ssize_t newPosition = (ssize_t)pFile->size + seekOffset;
			if (newPosition < 0 || newPosition >= pFile->size)
			{
				SG_LOG_ERROR("seeking exceed the file boundary!");
				return false;
			}
			pFile->memory.cursor = (size_t)newPosition;
		}
		break;
		}
		return true;
	}

	static ssize_t memory_stream_get_seek_position(const FileStream* pFile)
	{
		return pFile->memory.cursor;
	}

	static ssize_t memory_stream_get_file_size(const FileStream* pFile)
	{
		return pFile->size;
	}

	static bool memory_stream_flush(FileStream* pFile)
	{
		// no option
		return true;
	}

	static bool memory_stream_is_at_end(const FileStream* pFile)
	{
		return pFile->memory.cursor == pFile->size;
	}

	static IFileSystem gMemoryFileIO =
	{
		NULL,
		memory_stream_close,
		memory_stream_read,
		memory_stream_write,
		memory_stream_seek,
		memory_stream_get_seek_position,
		memory_stream_get_file_size,
		memory_stream_flush,
		memory_stream_is_at_end
	};

	/////////////////////////////////////////////////////////////////////////////
	/// File Stream Functions 
	/////////////////////////////////////////////////////////////////////////////

	static bool file_stream_open(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
	{
		return platform_open_file(resourceDir, fileName, mode, pOut);
	}

	static bool file_stream_close(FileStream* pFile)
	{
		return platform_close_file(pFile);
	}

	static size_t file_stream_read(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes)
	{
		if (!(pFile->mode & SG_FM_READ))
		{
			SG_LOG_WARNING("Attempting to read to a unreadable file!(with mode %u)", pFile->mode);
			return 0;
		}

		size_t bytesRead = fread(outputBuffer, 1, bufferSizeInBytes, pFile->file);
		if (bytesRead != bufferSizeInBytes)
		{
			if (ferror(pFile->file) != 0)
			{
				SG_LOG_WARNING("Error reading from system FileStream: %s", strerror(errno));
			}
		}
		return bytesRead;
	}

	static size_t file_stream_write(FileStream* pFile, const void* sourceBuffer, size_t byteCount)
	{
		if ((pFile->mode & (SG_FM_WRITE | SG_FM_APPEND)) == 0)
		{
			SG_LOG_WARNING("Writing to FileStream with mode %u", pFile->mode);
			return 0;
		}

		size_t bytesWritten = fwrite(sourceBuffer, 1, byteCount, pFile->file);
		if (bytesWritten != byteCount)
		{
			if (ferror(pFile->file) != 0)
			{
				SG_LOG_WARNING("Error writing to system FileStream: %s", strerror(errno));
			}
		}
		return bytesWritten;
	}

	static bool file_stream_seek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
	{
		if ((pFile->mode & SG_FM_BINARY) == 0 && baseOffset != SG_SBO_START_OF_FILE)
		{
			SG_LOG_WARNING("Text-mode FileStreams only support SBO_START_OF_FILE");
			return false;
		}

		int origin = SEEK_SET; // to the beginning of the file
		switch (baseOffset)
		{
		case SG_SBO_START_OF_FILE:    origin = SEEK_SET; break;
		case SG_SBO_CURRENT_POSITION: origin = SEEK_CUR; break;
		case SG_SBO_END_OF_FILE:      origin = SEEK_END; break;
		}

		return fseek(pFile->file, (long)seekOffset, origin) == 0;
	}

	static ssize_t file_stream_get_seek_position(const FileStream* pFile)
	{
		long int result = ftell(pFile->file);
		if (result == -1L)
		{
			SG_LOG_WARNING("Error getting seek position in FileStream: %i", errno);
		}
		return result;
	}

	static ssize_t file_stream_get_file_size(const FileStream* pFile)
	{
		return pFile->size;
	}

	static bool file_stream_flush(FileStream* pFile)
	{
		if (fflush(pFile->file) == EOF)
		{
			SG_LOG_WARNING("Error flushing system FileStream: %s", strerror(errno));
			return false;
		}
		return true;
	}

	static bool file_stream_is_at_end(const FileStream* pFile)
	{
		return feof(pFile->file) != 0;
	}

	static IFileSystem gSystemFileIO =
	{
		file_stream_open,
		file_stream_close,
		file_stream_read,
		file_stream_write,
		file_stream_seek,
		file_stream_get_seek_position,
		file_stream_get_file_size,
		file_stream_flush,
		file_stream_is_at_end
	};

	IFileSystem* pSystemFileIO = &gSystemFileIO;

	bool sgfs_open_stream_from_path(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
	{
		IFileSystem* io = gResourceDirectories[resourceDir].pIO;
		if (!io)
		{
			SG_LOG_ERROR("Trying to get an unset resource directory '%d', make sure the resourceDirectory is set on start of the application", resourceDir);
			return false;
		}
		return io->open(io, resourceDir, fileName, mode, pOut);
	}

	bool sgfs_open_stream_from_memory(const void* pBuffer, size_t bufferSize, FileMode mode, bool owner, FileStream* pOut)
	{
		FileStream stream = {};
		stream.memory.cursor = 0;
		stream.memory.pBuffer = (uint8_t*)pBuffer;
		stream.memory.owner = owner;
		stream.size = bufferSize;
		stream.mode = mode;
		stream.pIO = &gMemoryFileIO;
		*pOut = stream;
		return true;
	}

	bool sgfs_close_stream(FileStream* stream)
	{
		return stream->pIO->close(stream);
	}

	/// return the byte size of the stream
	size_t sgfs_read_from_stream(FileStream* pFile, void* pOutputBuffer, size_t bufferSizeInbytes)
	{
		return pFile->pIO->read(pFile, pOutputBuffer, bufferSizeInbytes);
	}

	size_t sgfs_write_to_stream(FileStream* pFile, const void* pSourceBuffer, size_t byteCount)
	{
		return pFile->pIO->write(pFile, pSourceBuffer, byteCount);
	}

	/// offset in the file stream 
	bool sgfs_seek_stream(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t byteOffset)
	{
		return pFile->pIO->seek(pFile, baseOffset, byteOffset);
	}

	ssize_t sgfs_get_offset_stream_position(const FileStream* pFile)
	{
		return pFile->pIO->get_seek_position(pFile);
	}

	/// get the size of the file. return -1 if the size if unknown
	ssize_t sgfs_get_stream_file_size(const FileStream* pFile)
	{
		return pFile->pIO->get_file_size(pFile);
	}

	bool sgfs_flush_stream(FileStream* pFile)
	{
		return pFile->pIO->flush(pFile);
	}

	bool sgfs_is_stream_at_end(const FileStream* pFile)
	{
		return pFile->pIO->is_at_end(pFile);
	}

	// forward declaration
	bool sgfs_create_directory(ResourceDirectory resourceDir);

	void sgfs_get_parent_path(const char* path, char* output)
	{
		size_t pathLength = strlen(path);
		ASSERT(pathLength != 0);

		const char directorySeparator = sgfs_get_directory_separator();
		const char forwardSlash = '/';    // forward slash is accepted on all platforms as a path component.

		// find the last seperater
		const char* dirSeperatorLoc = strrchr(path, directorySeparator);
		if (dirSeperatorLoc == NULL)
		{
			dirSeperatorLoc = strrchr(path, forwardSlash);
			if (dirSeperatorLoc == NULL)
			{
				return;
			}
		}

		const size_t outputLength = pathLength - strlen(dirSeperatorLoc);
		strncpy(output, path, outputLength);
		output[outputLength] = '\0';
	}

	const char* sgfs_get_resource_directory(ResourceDirectory resourceDir)
	{
		const ResourceDirectoryInfo* rdInfo = &gResourceDirectories[resourceDir];

		if (rdInfo->isBundled || !rdInfo->pIO)
		{
			SG_LOG_IF(SG_LOG_LEVEL_ERROR, !strlen(rdInfo->path), "Trying to get an unset resource directory '%d', make sure the resourceDirectory is set on start of the application", resourceDir);
			ASSERT(strlen(rdInfo->path) != 0);
		}
		return rdInfo->path;
	}

	void sgfs_set_path_for_resource_dir(IFileSystem* pIO, ResourceMount mount, ResourceDirectory resourceDir, const char* bundledFolder)
	{
		ASSERT(pIO);
		ResourceDirectoryInfo* rdInfo = &gResourceDirectories[resourceDir];

		if (strlen(rdInfo->path) != 0)
		{
			SG_LOG_WARNING("Resource directory {%d} already set on:'%s'", resourceDir, rdInfo->path);
			return;
		}

		if (SG_RM_CONTENT == mount)
			rdInfo->isBundled = true; // it is a bundle(a folder)

		char resourcePath[SG_MAX_FILEPATH] = { 0 };
		sgfs_append_path_component(pIO->get_resource_mount(mount) ? pIO->get_resource_mount(mount) : "", bundledFolder, resourcePath);
		strncpy(rdInfo->path, resourcePath, SG_MAX_FILEPATH); // set the file path for resource dir
		rdInfo->pIO = pIO;

		if (!rdInfo->isBundled)
		{
			if (!sgfs_create_directory(resourceDir))
			{
				SG_LOG_ERROR("Could not create directory '%s' in filesystem", resourcePath);
			}
		}
	}

	void sgfs_append_path_component(const char* basePath, const char* pathComponent, char* output)
	{
		const size_t componentLength = strlen(pathComponent);
		const size_t baseLength = strlen(basePath);
		const size_t maxPathLength = baseLength + componentLength + 1;    // + 1 due to a possible added directory slash.

		SG_LOG_IF(SG_LOG_LEVEL_ERROR, maxPathLength >= SG_MAX_FILEPATH, "Component path length '%d' greater than FS_MAX_PATH", maxPathLength);
		ASSERT(maxPathLength < SG_MAX_FILEPATH);

		strncpy(output, basePath, baseLength);
		size_t newPathLength = baseLength;
		output[baseLength] = '\0';

		if (componentLength == 0)
			return;

		const char directorySeparator = sgfs_get_directory_separator();
		const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.

		if (newPathLength != 0 && output[newPathLength - 1] != directorySeparator)
		{
			// Append a trailing slash to the directory.
			strncat(output, &directorySeparator, 1);
			newPathLength += 1;
			output[newPathLength] = '\0';
		}

		// ./ or .\ means current directory
		// ../ or ..\ means parent directory.
		for (size_t i = 0; i < componentLength; i += 1)
		{
			if ((pathComponent[i] == directorySeparator || pathComponent[i] == forwardSlash) &&
				newPathLength != 0 && output[newPathLength - 1] != directorySeparator)
			{
				// We've encountered a new directory.
				strncat(output, &directorySeparator, 1);
				newPathLength += 1;
				output[newPathLength] = '\0';
				continue;
			}
			else if (pathComponent[i] == '.')
			{
				size_t j = i + 1;
				if (j < componentLength)
				{
					if (pathComponent[j] == directorySeparator || pathComponent[j] == forwardSlash)
					{
						// ./, so it's referencing the current directory.
						// We can just skip it.
						i = j;
						continue;
					}
					else if (
						pathComponent[j] == '.' && ++j < componentLength &&
						(pathComponent[j] == directorySeparator || pathComponent[j] == forwardSlash))
					{
						// ../, so referencing the parent directory.

						if (newPathLength > 1 && output[newPathLength - 1] == directorySeparator)
						{
							// Delete any trailing directory separator.
							newPathLength -= 1;
						}

						// Backtrack until we come to the next directory separator
						for (; newPathLength > 0; newPathLength -= 1)
						{
							if (output[newPathLength - 1] == directorySeparator)
							{
								break;
							}
						}

						i = j;    // Skip past the ../
						continue;
					}
				}
			}

			output[newPathLength] = pathComponent[i];
			newPathLength += 1;
			output[newPathLength] = '\0';
		}

		if (output[newPathLength - 1] == directorySeparator)
		{
			// Delete any trailing directory separator.
			newPathLength -= 1;
		}

		output[newPathLength] = '\0';
	}

	void sgfs_append_path_extension(const char* basePath, const char* newExtension, char* output)
	{
		size_t extensionLength = strlen(newExtension);
		const size_t baseLength = strlen(basePath);
		const size_t maxPathLength = baseLength + extensionLength + 1;    // + 1 due to a possible added directory slash.

		SG_LOG_IF(SG_LOG_LEVEL_ERROR, maxPathLength >= SG_MAX_FILEPATH, "Extension path length '%d' greater than FS_MAX_PATH", maxPathLength);
		ASSERT(maxPathLength < SG_MAX_FILEPATH);

		strncpy(output, basePath, baseLength);

		if (extensionLength == 0)
		{
			return;
		}

		const char directorySeparator = sgfs_get_directory_separator();
		const char forwardSlash = '/';    // Forward slash is accepted on all platforms as a path component.

		// Extension validation
		for (size_t i = 0; i < extensionLength; i += 1)
		{
			SG_LOG_IF(SG_LOG_LEVEL_ERROR, newExtension[i] == directorySeparator || newExtension[i] == forwardSlash, "Extension '%s' contains directory specifiers", newExtension);
			ASSERT(newExtension[i] != directorySeparator && newExtension[i] != forwardSlash);
		}
		SG_LOG_IF(SG_LOG_LEVEL_ERROR, newExtension[extensionLength - 1] == '.', "Extension '%s' ends with a '.' character", newExtension);


		if (newExtension[0] == '.')
		{
			newExtension += 1;
			extensionLength -= 1;
		}

		strncat(output, ".", 1);
		strncat(output, newExtension, extensionLength);
		output[strlen(output)] = '\0';
	}

}
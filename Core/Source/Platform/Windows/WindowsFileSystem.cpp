#include "Interface/IFileSystem.h"
#include "Interface/ILog.h"

#include <ShlObj_core.h>

namespace SG
{

	static bool gInitialized = false;
	static const char* gResourceMounts[SG_RM_COUNT];

	static char gApplicationPath[SG_MAX_FILEPATH] = {};
	static char gDocumentsPath[SG_MAX_FILEPATH] = {};

	const char* get_resource_mount(ResourceMount mount)
	{
		return gResourceMounts[mount];
	}

	template <typename T>
	static inline T with_UTF16_path(const char* path, T(*function)(const wchar_t*))
	{
		size_t len = strlen(path);
		auto* buffer = (wchar_t*)alloca((len + 1) * sizeof(wchar_t));

		size_t resultLength = MultiByteToWideChar(CP_UTF8, 0, path, (int)len, buffer, (int)len);
		buffer[resultLength] = 0;

		return function(buffer);
	}

	// using c-file io
	// TODO: maybe use fstream in some day
	bool platform_open_file(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
	{
		const char* resourcePath = sgfs_get_resource_directory(resourceDir);
		char filePath[SG_MAX_FILEPATH] = {};
		sgfs_append_path_component(resourcePath, fileName, filePath);

		// path utf-16 conversion
		size_t filePathLen = strlen(filePath);
		auto* pathStr = (wchar_t*)alloca((filePathLen + 1) * sizeof(wchar_t));
		size_t pathStrLength =
			MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);
		pathStr[pathStrLength] = 0; /* '\0' */

		// mode string utf-16 conversion
		const char* modeStr = sgfs_file_mode_to_string(mode);
		wchar_t modeWStr[4] = {};
		mbstowcs(modeWStr, modeStr, 4);

		FILE* fp = NULL;
		if (mode & SG_FM_ALLOW_READ)
		{
			// use converted wchar to open the path
			fp = _wfsopen(pathStr, modeWStr, _SH_DENYWR /*¾Ü¾øÐ´*/);
		}
		else
		{
			_wfopen_s(&fp, pathStr, modeWStr);
		}

		if (fp)
		{
			*pOut = {};
			pOut->file = fp;
			pOut->mode = mode;
			pOut->pIO = pSystemFileIO;

			pOut->size = -1;
			if (fseek(pOut->file, 0, SEEK_END) == 0) // successfully jump to the end of the file
			{
				pOut->size = ftell(pOut->file);
				rewind(pOut->file); // back to the beginning of the file
			}
			return true;
		}
		else
		{
			SG_LOG_ERROR("Error opening file: %s -- %s (error: %s)", filePath, modeStr, strerror(errno));
		}
		return false;
	}

	bool platform_close_file(FileStream* pFile)
	{
		if (fclose(pFile->file) == EOF)
		{
			SG_LOG_ERROR("Error closing system FileStream", errno);
			return false;
		}
		return true;
	}

	static bool sgfs_is_directory_exists(const char* path)
	{
		return with_UTF16_path<bool>(path, [](const wchar_t* pathStr)
			{
				DWORD attributes = GetFileAttributesW(pathStr);
				return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
			});
	}

	static bool sgfs_create_directory(const char* path)
	{
		if (sgfs_is_directory_exists(path))
			return true;

		char parentPath[SG_MAX_FILEPATH] = { 0 };
		sgfs_get_parent_path(path, parentPath);
		if (parentPath[0] != 0)
		{
			if (!sgfs_is_directory_exists(parentPath)) // create directories in recursion
			{
				sgfs_create_directory(parentPath);
			}
		}
		return with_UTF16_path<bool>(path, [](const wchar_t* pathStr)
			{
				return ::CreateDirectoryW(pathStr, NULL) ? true : false;
			});
	}

	bool sgfs_create_directory(ResourceDirectory resoureceDir)
	{
		return sgfs_create_directory(sgfs_get_resource_directory(resoureceDir));
	}

	bool sgfs_init_file_system(FileSystemInitDescription* pDesc)
	{
		if (gInitialized)
		{
			// logging 
			return true;
		}
		ASSERT(pDesc);
		pSystemFileIO->get_resource_mount = get_resource_mount;

		// get the application directory
		wchar_t utf16Path[SG_MAX_FILEPATH];
		GetModuleFileNameW(0, utf16Path, SG_MAX_FILEPATH);
		char applicationFilePath[SG_MAX_FILEPATH] = {};
		WideCharToMultiByte(CP_UTF8, 0, utf16Path, -1, applicationFilePath, MAX_PATH, NULL, NULL);
		sgfs_get_parent_path(applicationFilePath, gApplicationPath);
		gResourceMounts[SG_RM_CONTENT] = gApplicationPath;
		gResourceMounts[SG_RM_DEBUG] = gApplicationPath;

		// get user directory
		PWSTR userDocuments = NULL;
		SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &userDocuments); // get the documents folder path
		WideCharToMultiByte(CP_UTF8, 0, userDocuments, -1, gDocumentsPath, MAX_PATH, NULL, NULL);
		CoTaskMemFree(userDocuments);
		gResourceMounts[SG_RM_SAVE_0] = gDocumentsPath;

		// override resource mounts
		for (uint32_t i = 0; i < SG_RM_COUNT; i++)
		{
			if (pDesc->resourceMounts[i])
			{
				gResourceMounts[i] = pDesc->resourceMounts[i];
			}
		}

		gInitialized = true;
		return true;
	}

	void sgfs_exit_file_system() // free the resources related to the file_system_api
	{
		gInitialized = false;
	}

}
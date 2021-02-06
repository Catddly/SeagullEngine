#include "Interface/ILog.h"

#include <io.h> // _isatty()

namespace SG
{

	void print_unicode(const char* str, bool error)
	{
		// if the output stream has been redirected, use fprintf instead of WriteConsoleW,
		// though it means that proper Unicode output will not work

		FILE* out = error ? stderr : stdout;
		if (!_isatty(_fileno(out))) // is it a terminal?
		{
			fprintf(out, "%s", str);
		}
		else
		{
			if (error)
				printf("Error: %s", str);
			else
				printf("%s", str);
		}
	}

	void output_debug_string_v(const char* str, va_list args)
	{
		const unsigned BUFFER_SIZE = 4096;
		char           buf[BUFFER_SIZE];

		vsprintf_s(buf, str, args);
		OutputDebugStringA(buf);
	}

	void output_debug_string(const char* str, ...)
	{
		va_list args;
		va_start(args, str);
		output_debug_string_v(str, args); // expand in this function
		va_end(args);
	}

	void failed_assert(const char* file, int line, const char* statement)
	{
		static bool debug = true;

		if (debug)
		{
			WCHAR str[1024];
			WCHAR message[1024];
			WCHAR wfile[1024];
			mbstowcs(message, statement, 1024);
			mbstowcs(wfile, file, 1024);
			wsprintfW(str, L"Failed: (%s)\n\nFile: %s\nLine: %d\n\n", message, wfile, line);

			if (IsDebuggerPresent())
			{
				wcscat(str, L"Debug?");
				int res = MessageBoxW(NULL, str, L"Assert failed", MB_YESNOCANCEL | MB_ICONERROR);
				if (res == IDYES)
				{
#if _MSC_VER >= 1400
					__debugbreak();
#else
					_asm int 0x03;
#endif
				}
				else if (res == IDCANCEL)
				{
					debug = false;
				}
			}
			else
			{
				// TODO : add stack trace dump
//#ifdef SG_STACKTRACE_DUMP
//				__debugbreak();
//#else
//				wcscat(str, L"Display more asserts?");
//				if (MessageBoxW(NULL, str, L"Assert failed", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2) != IDYES)
//				{
//					debug = false;
//				}
//#endif
			}
		}
	}

}
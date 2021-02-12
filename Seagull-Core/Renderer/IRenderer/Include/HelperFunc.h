#pragma once

#define SG_PLATFORM_WINDOWS

// for Nvidia's GPU
#if defined(SG_PLATFORM_WINDOWS) && !defined(DURANGO)
	#include "../../../Core/Third-party/Include/nvapi/nvapi.h"
	#define NVAPI_SUPPORTED
#else
	typedef enum NvAPI_Status
	{
		NVAPI_OK = 0,          // !< Success. Request is completed.
		NVAPI_ERROR = -1,      // !< Generic error
	} NvAPI_Status;
#endif

// for AMD's GPU
#if defined(SG_PLATFORM_WINDOWS) && !defined(DURANGO)
#include "../../../Core/Third-party/Include/ags/ags_lib/inc/amd_ags.h"
#define AMDAGS
#else
	enum AGSReturnCode
	{
		AGS_SUCCESS,                    ///< Successful function call
		AGS_FAILURE,                    ///< Failed to complete call for some unspecified reason
		AGS_INVALID_ARGS,               ///< Invalid arguments into the function
		AGS_OUT_OF_MEMORY,              ///< Out of memory when allocating space internally
		AGS_MISSING_D3D_DLL,            ///< Returned when a D3D dll fails to load
		AGS_LEGACY_DRIVER,              ///< Returned if a feature is not present in the installed driver
		AGS_NO_AMD_DRIVER_INSTALLED,    ///< Returned if the AMD GPU driver does not appear to be installed
		AGS_EXTENSION_NOT_SUPPORTED,    ///< Returned if the driver does not support the requested driver extension
		AGS_ADL_FAILURE,                ///< Failure in ADL (the AMD Display Library)
		AGS_DX_FAILURE                  ///< Failure from DirectX runtime
	};
#endif


#if defined(AMDAGS)
	static AGSContext* pAgsContext = NULL;
	static AGSGPUInfo  gAgsGpuInfo = {};
#endif

	static NvAPI_Status nvapiInit()
	{
#if defined(NVAPI_SUPPORTED)
		return NvAPI_Initialize();
#endif

		return NvAPI_Status::NVAPI_OK;
	}

	static void nvapiExit()
	{
#if defined(NVAPI_SUPPORTED)
		NvAPI_Unload();
#endif
	}

	static void nvapiPrintDriverInfo()
	{
#if defined(NVAPI_SUPPORTED)
		NvU32 driverVersion = 0;
		NvAPI_ShortString buildBranch = {};
		NvAPI_Status status = NvAPI_SYS_GetDriverAndBranchVersion(&driverVersion, buildBranch);
		if (NvAPI_Status::NVAPI_OK == status)
		{
			SG_LOG_INFO("NVIDIA Display Driver Version %u", driverVersion);
			SG_LOG_INFO("NVIDIA Build Branch %s", buildBranch);
		}
#endif
	}

	static AGSReturnCode agsInit()
	{
#if defined(AMDAGS)
		AGSConfiguration config = {};
		return agsInit(&pAgsContext, &config, &gAgsGpuInfo);
#endif

		return AGS_SUCCESS;
	}

	static void agsExit()
	{
#if defined(AMDAGS)
		agsDeInit(pAgsContext);
#endif
	}

	static void agsPrintDriverInfo()
	{
#if defined(AMDAGS)
		if (pAgsContext)
		{
			SG_LOG_INFO("AMD Display Driver Version %u", gAgsGpuInfo.driverVersion);
			SG_LOG_INFO("AMD Radeon Software Version %s", gAgsGpuInfo.radeonSoftwareVersion);
		}
#endif
	}

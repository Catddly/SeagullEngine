project "spirv-cross"
	kind "StaticLib"
	language "C++"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.cpp",
		"include/SpirvTools.h"
	}

	includedirs
	{
		"include",
		"../../../Source/",
		"../glm/",
		"../eastl/"
	}

	filter "system:windows"
		systemversion "latest"
		defines 
		{
			"_CRT_SECURE_NO_WARNINGS"
		}

	filter "configurations:Debug-Vulkan"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release-Vulkan"
		runtime "Release"
		optimize "on"
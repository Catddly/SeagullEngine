project "gainput"
	kind "StaticLib"
	language "C++"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",
		"src/**.mm",
	}

	includedirs
	{
		"include",
		"../../../Source/",
		"../eastl/",
		"../glm/"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"_CRT_NONSTDC_NO_DEPRECATE"
	}

	filter "system:windows"
		systemversion "latest"
		defines 
		{
			"_CRT_SECURE_NO_WARNINGS",
			"SG_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug-Vulkan"
		defines
		{
			"SG_DEBUG"
		}
		runtime "Debug"
		symbols "on"

	filter "configurations:Release-Vulkan"
		defines
		{
			"SG_RELEASE"
		}
		runtime "Release"
		optimize "on"
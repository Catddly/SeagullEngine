project "RendererVulkan"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"Source/**.h",
		"Source/**.cpp"
	}

	includedirs
	{
		"Include",
		"../IRenderer/Include/",
        "../../Core/Third-party/Include/eastl/",
        "../../Core/Third-party/Include/tinyImageFormat/",
		"../../Core/Source/",
		"../../Core/Third-party/Include/glm/",
		"../../Core/Third-party/Include/spirv-cross/"
	}

	-- link libraries
	links
	{
		"libs/vulkan-1.lib",
		"spirv-cross"
	}

	filter "system:windows"
		systemversion "latest"
		defines 
		{
			"_CRT_SECURE_NO_WARNINGS",
			"SG_GRAPHIC_API_VULKAN",
			"SG_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines
		{
			"SG_DEBUG",
			"SG_VULKAN_USE_DEBUG_UTILS_EXTENSION"
		}
		runtime "Debug"
		symbols "on"
		
	filter "configurations:Release"
		defines
		{
			"SG_RELEASE"
		}
		runtime "Release"
		optimize "on"
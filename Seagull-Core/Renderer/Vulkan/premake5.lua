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
		"include",
		"../IRenderer/Include/",
		"%{IncludeDir.eastl}",
		"%{IncludeDir.glm}",
		"../Third-party/VulkanMemoryAllocator/",
		"../../Core/Third-party/Include",
		"../../Core/Source/"
	}

	-- link libraries
	links
	{
		-- "eastl"
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
			"SG_DEBUG"
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
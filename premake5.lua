workspace "Seagull"
    architecture "x64"
    startproject "Sandbox"

    configurations
    {
        "Debug", 
		"Release",
    }

-- Debug-windows-x64
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
IncludeDir = { }
IncludeDir["eastl"] = "Seagull-Core/Core/Third-party/Include/eastl"
IncludeDir["mimalloc"] = "Seagull-Core/Core/Third-party/Include/mimalloc"
IncludeDir["glm"] = "Seagull-Core/Core/Third-party/Include/glm"
IncludeDir["tinyImageFormat"] = "Seagull-Core/Core/Third-party/Include/tinyImageFormat"

-- Renderers
IncludeDir["RendererVulkan"] = "Seagull-Core/Renderer/Vulkan"

copyfiledir = "\"$(SolutionDir)Bin\\" .. outputdir .. "\\%{prj.name}\\$(ProjectName).dll\""
copylibdir  = "\"$(ProjectDir)bin\\"
copydstdir  = "\"$(SolutionDir)Bin\\" .. outputdir .. "\\Sandbox\\\""

group "Dependencies"

    include "Seagull-Core/Core/Third-party/Include/mimalloc"
    include "Seagull-Core/Core/Third-party/Include/eastl"

group ""

project "Seagull-Core"
    location "%{prj.name}/Core"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    -- bin/Debug-windows-x64/Seagull Core
    targetdir ("Bin/" .. outputdir .. "/%{prj.name}")
    -- bin-int/Debug-windows-x64/Seagull Core
    objdir    ("Bin-int/" .. outputdir .. "/%{prj.name}")

    -- include files
    files
	{
		"%{prj.name}/Core/Source/**.h",
		"%{prj.name}/Core/Source/**.cpp",
	}

    -- define macros
    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    -- include directories
    includedirs
    {
        "%{prj.name}/Core/Source",
        "%{IncludeDir.mimalloc}",
        "%{IncludeDir.eastl}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.tinyImageFormat}"
    }
    
    -- link libraries
    links
    {
        "mimalloc",
        "eastl",
    }

filter "system:windows"
    systemversion "latest"
    defines
    {
        "SG_PLATFORM_WINDOWS",
        -- "SG_GRAPHIC_API_D3D12_SUPPORTED",
        "SG_GRAPHIC_API_VULKAN_SUPPORTED"
    }

filter "configurations:Debug"
    defines 
    {
        "SG_DEBUG",
        "SG_ENABLE_ASSERT",
    }
    runtime "Debug"
    symbols "on"

filter "configurations:Release"
    defines 
    {
        "SG_RELEASE",
    }
    runtime "Release"
    optimize "on"

group "Renderer"

    project "IRenderer"
        location "Seagull-Core/Renderer/IRenderer"
        kind "None"
        language "C++"
        cppdialect "C++17"
        staticruntime "on"

        files
        {
            "Seagull-Core/Renderer/IRenderer/Include/**.h"
        }

    include "Seagull-Core/Renderer/Vulkan"

group ""

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    -- bin/Debug-windows-x64/Seagull Core
    targetdir ("Bin/" .. outputdir .. "/%{prj.name}")
    -- bin-int/Debug-windows-x64/Seagull Core
    objdir    ("Bin-int/" .. outputdir .. "/%{prj.name}")

    -- include files
    files
	{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp"
	}

    -- include directories
    includedirs
    {
        "Seagull-Core/Core/Source/",
        "%{IncludeDir.eastl}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.RendererVulkan}"
    }

    -- link libraries
    links
    {
        "Seagull-Core",
        "RendererVulkan"
    }

filter "system:windows"
    systemversion "latest"
    defines "SG_PLATFORM_WINDOWS"

filter "configurations:Debug"
    defines "SG_DEBUG"
    runtime "Debug"
    symbols "on"
    -- postbuildcommands { "editbin/subsystem:console $(OutDir)$(ProjectName).exe" } -- enable console

filter "configurations:Release"
    defines "SG_RELEASE"
    runtime "Release"
    optimize "on"
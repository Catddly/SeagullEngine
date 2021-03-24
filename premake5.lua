workspace "Seagull"
    architecture "x64"
    startproject "Sandbox"

    configurations
    {
        "Debug-Vulkan", 
		"Release-Vulkan"
    }

-- Debug-windows-x64
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
IncludeDir = { }
IncludeDir["eastl"] = "Seagull-Core/Core/Third-party/Include/eastl"
IncludeDir["mimalloc"] = "Seagull-Core/Core/Third-party/Include/mimalloc"
IncludeDir["glm"] = "Seagull-Core/Core/Third-party/Include/glm"
IncludeDir["tinyImageFormat"] = "Seagull-Core/Core/Third-party/Include/tinyImageFormat"
IncludeDir["spirv-cross"] = "Seagull-Core/Core/Third-party/Include/spirv-cross"
IncludeDir["ImGui"] = "Seagull-Core/Core/Third-party/Include/ImGui"
IncludeDir["fontStash"] = "Seagull-Core/Core/Third-party/Include/fontStash"
IncludeDir["gainput"] = "Seagull-Core/Core/Third-party/Include/gainput"

-- Renderers
IncludeDir["RendererVulkan"] = "Seagull-Core/Renderer/Vulkan"

copyfiledir = "\"$(SolutionDir)Bin\\" .. outputdir .. "\\%{prj.name}\\$(ProjectName).dll\""
copylibdir  = "\"$(ProjectDir)bin\\"
copydstdir  = "\"$(SolutionDir)Bin\\" .. outputdir .. "\\Sandbox\\\""

group "Dependencies"

    include "Seagull-Core/Core/Third-party/Include/mimalloc"
    include "Seagull-Core/Core/Third-party/Include/eastl"
    include "Seagull-Core/Core/Third-party/Include/ImGui"

group ""

group "Tools"

    include "Seagull-Core/Core/Third-party/Include/spirv-cross"
    include "Seagull-Core/Core/Third-party/Include/gainput"

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
        "%{IncludeDir.tinyImageFormat}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.fontStash}",
        "%{IncludeDir.gainput}",
        "Seagull-Core/Renderer/IRenderer/"
    }
    
    -- link libraries
    links
    {
        "mimalloc",
        "eastl",
        "ImGui",
        "gainput"
    }

filter "system:windows"
    systemversion "latest"
    defines
    {
        "SG_PLATFORM_WINDOWS",
        "SG_EDITOR_ENABLE_DOCKSPACE"
        -- "SG_GRAPHIC_API_D3D12_SUPPORTED",
    }

filter "configurations:Debug-Vulkan"
    defines 
    {
        "SG_DEBUG",
        "SG_ENABLE_ASSERT",
        "SG_GRAPHIC_API_VULKAN"
    }
    runtime "Debug"
    symbols "on"

filter "configurations:Release-Vulkan"
    defines 
    {
        "SG_RELEASE",
        "SG_GRAPHIC_API_VULKAN"
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

        filter "configurations:Debug-Vulkan"
        defines 
        {
            "SG_DEBUG",
            "SG_ENABLE_ASSERT",
            "SG_GRAPHIC_API_VULKAN"
        }
        runtime "Debug"
        symbols "on"

        filter "configurations:Release-Vulkan"
        defines 
        {
            "SG_RELEASE",
            "SG_GRAPHIC_API_VULKAN"
        }
        runtime "Release"
        optimize "on"

    include "Seagull-Core/Renderer/Vulkan"

group ""

project "Sandbox"
    location "User/Sandbox"
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
		"User/%{prj.name}/Source/**.h",
		"User/%{prj.name}/Source/**.cpp"
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
        "RendererVulkan",
        "eastl"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
		"_CRT_NONSTDC_NO_DEPRECATE"
    }

filter "system:windows"
    systemversion "latest"
    defines "SG_PLATFORM_WINDOWS"

filter "configurations:Debug-Vulkan"
    defines
    {
         "SG_DEBUG",
         "SG_GRAPHIC_API_VULKAN"
    }
    runtime "Debug"
    symbols "on"
    -- postbuildcommands { "editbin/subsystem:console $(OutDir)$(ProjectName).exe" } -- enable console

filter "configurations:Release-Vulkan"
    defines 
    {
        "SG_RELEASE",
        "SG_GRAPHIC_API_VULKAN"
    }
    runtime "Release"
    optimize "on"


project "Editor"
    location "User/Editor"
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
		"User/%{prj.name}/Source/**.h",
		"User/%{prj.name}/Source/**.cpp"
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
        "RendererVulkan",
        "eastl"
    }

filter "system:windows"
    systemversion "latest"
    defines "SG_PLATFORM_WINDOWS"

filter "configurations:Debug-Vulkan"
    defines
    {
         "SG_DEBUG",
         "SG_GRAPHIC_API_VULKAN",
         "SG_EDITOR_ENABLE_DOCKSPACE"
    }
    runtime "Debug"
    symbols "on"

filter "configurations:Release-Vulkan"
    defines 
    {
        "SG_RELEASE",
        "SG_GRAPHIC_API_VULKAN",
        "SG_EDITOR_ENABLE_DOCKSPACE"
    }
    runtime "Release"
    optimize "on"
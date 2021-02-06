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
IncludeDir["eastl"] = "Core/Third-party/Include/eastl"
IncludeDir["mimalloc"] = "Core/Third-party/Include/mimalloc"

copyfiledir = "\"$(SolutionDir)Bin\\" .. outputdir .. "\\%{prj.name}\\$(ProjectName).dll\""
copylibdir  = "\"$(ProjectDir)bin\\"
copydstdir  = "\"$(SolutionDir)Bin\\" .. outputdir .. "\\Sandbox\\\""

group "Dependencies"

    include "Core/Third-party/Include/mimalloc"
    include "Core/Third-party/Include/eastl"

group ""

project "Seagull-Core"
    location "Core"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    -- bin/Debug-windows-x64/Seagull Core
    targetdir ("Bin/" .. outputdir .. "/%{prj.name}")
    -- bin-int/Debug-windows-x64/Seagull Core
    objdir    ("Bin-int/" .. outputdir .. "/%{prj.name}")

    -- precompile header

    -- include files
    files
	{
		"Core/Source/**.h",
		"Core/Source/**.cpp",
	}

    -- define macros
    defines
    {
        -- "_CRT_SECURE_NO_WARNINGS"
    }

    -- include directories
    includedirs
    {
        "Core/Source",
        "%{IncludeDir.mimalloc}",
        "%{IncludeDir.eastl}"
    }
    
    -- link libraries
    links
    {
        "mimalloc",
        "eastl"
    }

filter "system:windows"
    systemversion "latest"
    defines
    {
        "SG_PLATFORM_WINDOWS",
        "SG_GRAPHIC_API_DX12",
        "SG_GRAPHIC_API_VULKAN"
    }

filter "configurations:Debug"
    defines 
    {
        "SG_DEBUG",
        "SG_ENABLE_ASSERT",
    }
    -- postbuildcommands
    -- {
    --     ("copy /Y " .. copyfiledir .. " " .. copydstdir .."")
    -- }
    runtime "Debug"
    symbols "on"

filter "configurations:Release"
    defines 
    {
        "SG_RELEASE",
    }
    -- postbuildcommands
    -- {
    --     ("copy /Y " .. copyfiledir .. " " .. copydstdir .."")
    -- }
    runtime "Release"
    optimize "on"


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
        "Core/Source",
        "%{IncludeDir.eastl}"
    }

    -- link libraries
    links
    {
        "Seagull-Core",
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
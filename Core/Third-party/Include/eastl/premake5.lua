project "eastl"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir)
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.cpp"
	}
	
	includedirs
	{
		"include"
	}

	links
	{
	}

	filter "system:windows"
		systemversion "latest"
		defines 
		{
			"_CRT_SECURE_NO_WARNINGS"
		}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		-- enable if you want to build a dll
		-- postbuildcommands
		-- {
		-- 	("copy /Y " .. copylibdir .. "$(ProjectName).dll\" " .. copydstdir ..""),
		-- 	("copy /Y " .. copylibdir .. "$(ProjectName)-redirect.dll\" " .. copydstdir .."")
		-- }

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
		-- postbuildcommands
		-- {
		-- 	("copy /Y " .. copylibdir .. "$(ProjectName).dll\" " .. copydstdir ..""),
		-- 	("copy /Y " .. copylibdir .. "$(ProjectName)-redirect.dll\" " .. copydstdir .."")
		-- }
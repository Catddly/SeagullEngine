project "mimalloc"
	kind "StaticLib"
	language "C"
	staticruntime "on"

	targetdir ("bin/")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/bitmap.inc.c",
		"src/stats.c",
		"src/random.c",
		"src/os.c",
		"src/arena.c",
		"src/region.c",
		"src/segment.c",
		"src/page.c",
		-- "src/page-queue.c",
		"src/alloc.c",
		"src/alloc-aligned.c",
		-- "src/alloc-override.c",
		"src/alloc-posix.c",
		"src/heap.c",
		"src/options.c",
		"src/init.c"
	}
	
	defines
	{
	}

	includedirs
	{
		"include"
	}

	links
	{
		"bin/mimalloc-redirect"
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
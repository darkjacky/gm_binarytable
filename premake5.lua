workspace "gmsv_binarytable"
    configurations { "Release" }
    location ("projects/" .. os.target())

-- 32-bit
project "gmsv_binarytable_32"
    kind "SharedLib"
    language "C++"
    architecture "x86"         -- 32-bit
    includedirs { "gmod-module-base/include/" }
    targetdir "build"       -- separate output folder
    symbols "On"
	targetname "gmsv_binarytable"
	targetextension ".dll"
	targetprefix ""

    filter "system:windows"
        targetsuffix "_win32"
        targetextension ".dll"
        staticruntime "On"
		
    filter { "configurations:Release", "system:windows" }
        optimize "Speed"
        linktimeoptimization "On"
		defines { "BUILDAVX512" }
	
    filter "system:linux"
        optimize "Speed"
        targetsuffix "_linux" -- gmod wants _linux not _linux32
        targetextension ".dll"
		-- Linux can work with BUILDAVX512 but it causes illegal instructions because the flags are compiling it weirdly. No idea how to fix it. If you do please make a pull request.

    filter { "configurations:Release", "system:not windows" }
        optimize "Speed"
        linktimeoptimization "On"

    filter {}
    files { "source/**.*" }

-- 64-bit
project "gmsv_binarytable_64"
    kind "SharedLib"
    language "C++"
    architecture "x86_64"      -- 64-bit
    includedirs { "gmod-module-base/include/" }
    targetdir "build"       -- separate output folder
    symbols "On"
	targetname "gmsv_binarytable"
	targetextension ".dll"
	targetprefix ""

    filter "system:windows"
        targetsuffix "_win64"
        targetextension ".dll"
        staticruntime "On"
		
    filter { "configurations:Release", "system:windows" }
        optimize "Speed"
        linktimeoptimization "On"
		defines { "BUILDAVX512" }
		
    filter "system:linux"
        optimize "Speed"
        targetsuffix "_linux64"
        targetextension ".dll"
		-- Linux can work with BUILDAVX512 but it causes illegal instructions because the flags are compiling it weirdly. No idea how to fix it. If you do please make a pull request.

    filter { "configurations:Release", "system:not windows" }
        optimize "Speed"
        linktimeoptimization "On"

    filter {}
    files { "source/**.*" }

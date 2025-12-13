workspace "gmsv_binarytable"
    configurations { "Release" }
    location ("projects/" .. os.target())

-- ========================
-- 32-bit project
-- ========================
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

    filter "system:macosx"
        targetsuffix "_osx32"
        targetextension ".dll"

    filter "system:linux"
        optimize "Speed"
        targetsuffix "_linux" -- gmod wants _linux not _linux32
        targetextension ".dll"
		defines { "BUILDAVX512" } -- broken causing illegal instructions on unsupported hardware. Needs a fix.
		buildoptions {
			"-mavx512f",
			"-mavx512vl",
			"-mavx512bw",
			"-mavx512dq",
			"-mvpclmulqdq",
			"-mpclmul"
		} -- Dont think this will cause any problems, perhaps it makes it faster even without enabling the AVX512 script

    filter { "configurations:Release", "system:windows" }
        optimize "Speed"
        linktimeoptimization "On"
        buildoptions { "/Ot", "/Ob2", "/Oi", "/Oy" }
		defines { "BUILDAVX512" }

    filter { "configurations:Release", "system:not windows" }
        optimize "Speed"
        linktimeoptimization "On"

    filter {}
    files { "source/**.*" }

-- ========================
-- 64-bit project
-- ========================
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

    filter "system:macosx"
        targetsuffix "_osx64"
        targetextension ".dll"

    filter "system:linux"
        optimize "Speed"
        targetsuffix "_linux64"
        targetextension ".dll"
		--defines { "BUILDAVX512" } -- broken causing illegal instructions on unsupported hardware. Needs a fix.
		buildoptions {
			"-mavx512f",
			"-mavx512vl",
			"-mavx512bw",
			"-mavx512dq",
			"-mvpclmulqdq",
			"-mpclmul"
		} -- Dont think this will cause any problems, perhaps it makes it faster even without enabling the AVX512 script

    filter { "configurations:Release", "system:windows" }
        optimize "Speed"
        linktimeoptimization "On"
        buildoptions { "/Ot", "/Ob2", "/Oi", "/Oy" }
		defines { "BUILDAVX512" }

    filter { "configurations:Release", "system:not windows" }
        optimize "Speed"
        linktimeoptimization "On"

    filter {}
    files { "source/**.*" }

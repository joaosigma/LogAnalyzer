cmake_minimum_required(VERSION 3.12.4 FATAL_ERROR)

get_property(BUILD_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if (NOT BUILD_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default build type: release" FORCE)
	message(STATUS "Setting build type to 'Release' as none was specified.")
endif()

option(BUILD_CONSOLE "Build console executable instead of shared library" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project (logAnalyzer DESCRIPTION "WCL Log Analyzer")

# compiler options

add_compile_definitions(CXXOPTS_NO_RTTI)

if (DEFINED MSVC)

	add_compile_options(/GR-)
	add_compile_options(/utf-8)
	add_compile_options(/permissive-)
	add_compile_options(/W3)
	add_compile_options(/WX)

	add_compile_options(/MP)
	add_compile_options(/Bv)

	add_compile_definitions(VC_EXTRALEAN)
	add_compile_definitions(WIN32_LEAN_AND_MEAN)

	add_compile_definitions(WINVER=0x0A00) # Windows 10
	add_compile_definitions(_WIN32_WINNT=0x0A00) # Windows 10

	add_compile_definitions(NOGDICAPMASKS)
	add_compile_definitions(NOVIRTUALKEYCODES)
	add_compile_definitions(NOWINMESSAGES)
	add_compile_definitions(NOWINSTYLES)
	add_compile_definitions(NOSYSMETRICS)
	add_compile_definitions(NOMENUS)
	add_compile_definitions(NOICONS)
	add_compile_definitions(NOKEYSTATES)
	add_compile_definitions(NOSYSCOMMANDS)
	add_compile_definitions(NORASTEROPS)
	add_compile_definitions(NOSHOWWINDOW)
	add_compile_definitions(OEMRESOURCE)
	add_compile_definitions(NOATOM)
	add_compile_definitions(NOCLIPBOARD)
	add_compile_definitions(NOCOLOR)
	add_compile_definitions(NOCTLMGR)
	add_compile_definitions(NODRAWTEXT)
	add_compile_definitions(NOGDI)
	add_compile_definitions(NOMB)
	add_compile_definitions(NOMEMMGR)
	add_compile_definitions(NOMETAFILE)
	add_compile_definitions(NOMINMAX)
	add_compile_definitions(NOOPENFILE)
	add_compile_definitions(NOSCROLL)
	add_compile_definitions(NOSERVICE)
	add_compile_definitions(NOSOUND)
	add_compile_definitions(NOTEXTMETRIC)
	add_compile_definitions(NOWH)
	add_compile_definitions(NOWINOFFSETS)
	add_compile_definitions(NOCOMM)
	add_compile_definitions(NOKANJI)
	add_compile_definitions(NOHELP)
	add_compile_definitions(NOPROFILER)
	add_compile_definitions(NODEFERWINDOWPOS)
	add_compile_definitions(NOMCX)

	add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS)

else()

	set(CMAKE_C_VISIBILITY_PRESET hidden)
	set(CMAKE_CXX_VISIBILITY_PRESET hidden)
	set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

	add_compile_options(-g)
	add_compile_options(-fdata-sections)
	add_compile_options(-ffunction-sections)
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>")
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fexceptions>")

	add_compile_options(-Wall)
	add_compile_options(-Werror)

	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		add_compile_options(-fdiagnostics-color=always)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		add_compile_options(-fcolor-diagnostics)
	endif ()

endif ()

# setup the project

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(third_party)
add_subdirectory(core)

if (BUILD_CONSOLE)
	add_subdirectory(console)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT console)
else()
	add_subdirectory(shared)
endif ()


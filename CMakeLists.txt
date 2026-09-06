cmake_minimum_required(VERSION 3.21)

project(
	HairPositioner
	VERSION 0.0.1
	LANGUAGES CXX
)

# ---------------------------------------------------------------------------
#  CommonLibSSE-NG configuration
#
#  ENABLE_SKYRIM_VR must stay OFF. NiSkinInstance::lock -- which this plugin
#  takes before swapping a skin partition -- is compiled out of NG when VR
#  support is enabled (VR has no such member). We target SE/AE only anyway.
# ---------------------------------------------------------------------------
set(ENABLE_SKYRIM_SE ON  CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON  CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS      OFF CACHE BOOL "" FORCE)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/CMakeLists.txt")
	message(FATAL_ERROR
		"extern/CommonLibSSE-NG is missing. Run setup.bat first (it clones CommonLibSSE-NG).")
endif()

add_subdirectory(extern/CommonLibSSE-NG CommonLibSSE-NG)
include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)

set(SOURCES
	src/main.cpp
	src/Positioner.cpp
	src/Positioner.h
	src/HairOffset.cpp
	src/HairOffset.h
	src/HairScene.cpp
	src/HairScene.h
	src/MeshPatch.cpp
	src/MeshPatch.h
	src/Pivot.cpp
	src/Pivot.h
	src/Settings.cpp
	src/Settings.h
	src/Console.cpp
	src/Console.h
	src/Events.cpp
	src/Events.h
	src/Papyrus.cpp
	src/Papyrus.h
	src/PCH.h
)

add_commonlibsse_plugin(
	${PROJECT_NAME}
	SOURCES ${SOURCES}
	NAME "HairPositioner"
	AUTHOR "qtuna"
	USE_ADDRESS_LIBRARY
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

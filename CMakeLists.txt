cmake_minimum_required(VERSION 3.22)

project(
	CharacterPanel
	VERSION 1.2.0
	LANGUAGES CXX
)

set(PROJECT_NAMESPACE MyPlugins::${PROJECT_NAME})
set(PROJECT_AUTHOR AmazingBuff)

option(ENABLE_SKYRIM_SE "Enable support for Skyrim SE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_AE "Enable support for Skyrim AE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_VR "Enable support for Skyrim VR in the dynamic runtime feature." OFF)
option(CHARACTER_PANEL_BUILD_PROBE "Build the opt-in M0 engine evidence probe." ON)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(cmake/packaging.cmake)

file(GLOB_RECURSE PLUGIN_SOURCES src/*.cpp)
file(GLOB_RECURSE PLUGIN_HEADERS src/*.h)

set(VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/plugin.h)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/plugin.h.in
	${VERSION_HEADER}
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

source_group(Source FILES ${PLUGIN_SOURCES})
source_group(Header FILES ${PLUGIN_HEADERS} ${VERSION_HEADER})

# The vcpkg detours port ships no CMake config; locate the header/lib like
# Community Shaders does.
find_path(DETOURS_INCLUDE_DIRS "detours/detours.h")
find_library(DETOURS_LIBRARY detours REQUIRED)

add_library(
	CharacterPanel
	SHARED
	${PLUGIN_SOURCES}
	${PLUGIN_HEADERS}
	${VERSION_HEADER}
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

target_compile_features(
	CharacterPanel
	PRIVATE
	cxx_std_23
)

# Align the MSVC toolset with the one vcpkg uses for dependencies (spdlog).
# CMake 3.31 drops the `version=` clause from CMAKE_GENERATOR_TOOLSET (the
# preset's `v143,version=14.44.35207` arrives here as a plain `v143`), which
# resolves to an older installed toolset (14.38) whose STL lacks the vectorized
# symbols 14.44 objects reference — spdlog.lib then fails to link with LNK2019
# on __std_find_*_trivial_pos_1. Pin the toolset per-target so every generated
# .vcxproj uses 14.44 regardless of the generator toolset clause.
set_target_properties(
	CharacterPanel
	PROPERTIES
	OUTPUT_NAME CharacterPanel
	VS_GLOBAL_VCToolsVersion "14.44.35207"
)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
	target_compile_options(
		CharacterPanel
		PRIVATE
		/sdl	# Enable Additional Security Checks
		/utf-8	# Set Source and Executable character sets to UTF-8
		/Zi	# Debug Information Format

		/permissive-	# Standards conformance
		/Zc:preprocessor	# Enable preprocessor conformance mode

		/wd4200 # nonstandard extension used : zero-sized array in struct/union

		$<$<CONFIG:DEBUG>:/ZI>
		$<$<CONFIG:RELEASE>:/Zi;/Zc:inline;/JMC-;/Ob3>
	)

	target_compile_definitions(
		CharacterPanel
		PRIVATE
		NOMINMAX
	)

	target_link_options(
		CharacterPanel
		PRIVATE
		$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>
		$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>
	)
endif()

target_include_directories(
	CharacterPanel
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${DETOURS_INCLUDE_DIRS}
)

set(BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE)

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_link_libraries(
	CharacterPanel
	PRIVATE
	spdlog::spdlog
	fmt::fmt
	CommonLibSSE::CommonLibSSE
	${DETOURS_LIBRARY}
	user32
	d3dcompiler
)

target_precompile_headers(
	CharacterPanel
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h
)

if(CHARACTER_PANEL_BUILD_PROBE)
	set(PROBE_VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/probe/probe_config.h)
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/tools/m0_probe/probe_config.h.in
		${PROBE_VERSION_HEADER}
		@ONLY
	)

	set(PROBE_VERSION_RESOURCE ${CMAKE_CURRENT_BINARY_DIR}/probe/probe_version.rc)
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/tools/m0_probe/probe_version.rc.in
		${PROBE_VERSION_RESOURCE}
		@ONLY
	)

	add_library(
		CharacterPanelProbe
		SHARED
		tools/m0_probe/probe.h
		tools/m0_probe/probe_main.cpp
		tools/m0_probe/probe_capture.cpp
		${PROBE_VERSION_HEADER}
		${PROBE_VERSION_RESOURCE}
	)

	target_compile_features(
		CharacterPanelProbe
		PRIVATE
		cxx_std_23
	)

	set_target_properties(
		CharacterPanelProbe
		PROPERTIES
		OUTPUT_NAME CharacterPanelProbe
		VS_GLOBAL_VCToolsVersion "14.44.35207"
	)

	if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
		target_compile_options(
			CharacterPanelProbe
			PRIVATE
			/sdl
			/utf-8
			/Zi
			/permissive-
			/Zc:preprocessor
			/wd4200
			$<$<CONFIG:DEBUG>:/ZI>
			$<$<CONFIG:RELEASE>:/Zi;/Zc:inline;/JMC-;/Ob3>
		)

		target_compile_definitions(
			CharacterPanelProbe
			PRIVATE
			NOMINMAX
		)

		target_link_options(
			CharacterPanelProbe
			PRIVATE
			$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>
			$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>
		)
	endif()

	target_include_directories(
		CharacterPanelProbe
		PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/probe
		${CMAKE_CURRENT_SOURCE_DIR}/tools/m0_probe
	)

	target_link_libraries(
		CharacterPanelProbe
		PRIVATE
		spdlog::spdlog
		fmt::fmt
		CommonLibSSE::CommonLibSSE
		user32
		ntdll
	)

	add_custom_target(
		CharacterPanelProbePackage
		COMMAND
		${CMAKE_COMMAND}
		-DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-DOUTPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR}/dist
		-DPROJECT_VERSION=${PROJECT_VERSION}
		-DDLL_PATH=$<TARGET_FILE:CharacterPanelProbe>
		-P
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/probe_packaging.cmake
		DEPENDS
		CharacterPanelProbe
		VERBATIM
	)
endif()

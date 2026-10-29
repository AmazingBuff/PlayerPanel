cmake_minimum_required(VERSION 3.22)

project(
	CharacterPanel
	VERSION 1.2.1
	LANGUAGES CXX
)

set(PROJECT_NAMESPACE MyPlugins::${PROJECT_NAME})
set(PROJECT_AUTHOR AmazingBuff)
set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Supported build configuration" FORCE)
set(CMAKE_SKIP_INSTALL_RULES ON)

option(ENABLE_SKYRIM_SE "Enable support for Skyrim SE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_AE "Enable support for Skyrim AE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_VR "Enable support for Skyrim VR in the dynamic runtime feature." OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(PLUGIN_SOURCES
	src/animation/behavior_graph/frame_trace.cpp
	src/animation/behavior_graph/independent_graph.cpp
	src/animation/behavior_graph/ownership_probe.cpp
	src/animation/behavior_graph/source_timeline.cpp
	src/animation/clip.cpp
	src/animation/controller.cpp
	src/animation/dac/effects.cpp
	src/animation/dac/native.cpp
	src/animation/dac/script_attachment.cpp
	src/animation/dac/session.cpp
	src/animation/event_clock.cpp
	src/animation/oar/hooks.cpp
	src/animation/settings.cpp
	src/character/character_manager.cpp
	src/character/equipment_state.cpp
	src/character/figure_audit.cpp
	src/character/held_equipment.cpp
	src/character/scene_graph.cpp
	src/character/scene_graph_capture.cpp
	src/input/input.cpp
	src/main.cpp
	src/panel/panel_monitor.cpp
	src/render/dx11/common_states.cpp
	src/render/dx11/d3d11_util.cpp
	src/render/pass/composite.cpp
	src/render/render_hook.cpp
	src/render/renderer.cpp
	src/render/shader_manager.cpp
	src/render/studio/figure_passes.cpp
	src/render/studio/light.cpp
)
file(GLOB_RECURSE PLUGIN_HEADERS CONFIGURE_DEPENDS src/*.h)

# HLSL sources live as real files under src/render/shaders and are embedded
# into a generated header at build time (the HLC reference-project pattern).
set(RENDER_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders)
set(RENDER_SHADER_EMBED
	composite.hlsl:Composite
)
set(RENDER_SHADER_FILES "")
foreach(RENDER_SHADER_ENTRY ${RENDER_SHADER_EMBED})
	string(REPLACE ":" ";" RENDER_SHADER_PAIR ${RENDER_SHADER_ENTRY})
	list(GET RENDER_SHADER_PAIR 0 RENDER_SHADER_FILE)
	list(APPEND RENDER_SHADER_FILES ${RENDER_SHADER_DIR}/${RENDER_SHADER_FILE})
endforeach()
set(GENERATED_SHADER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/render/shader_sources.h)
add_custom_command(
	OUTPUT ${GENERATED_SHADER_HEADER}
	COMMAND ${CMAKE_COMMAND}
		-DSHADER_DIR=${RENDER_SHADER_DIR}
		-DOUTPUT_HEADER=${GENERATED_SHADER_HEADER}
		"-DEMBED_ENTRIES=${RENDER_SHADER_EMBED}"
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
	DEPENDS ${RENDER_SHADER_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
	COMMENT "Embedding HLSL sources (render/shaders)"
	VERBATIM
)

set(VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/plugin.h)

# Keep build identity for game results returned from another machine.
set(CHARACTER_PANEL_BUILD_LABEL "scenegraph" CACHE STRING "SceneGraph build identity label")

execute_process(
	COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse --short=10 HEAD
	OUTPUT_VARIABLE CHARACTER_PANEL_SOURCE_REVISION
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
execute_process(
	COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} status --porcelain
	OUTPUT_VARIABLE CHARACTER_PANEL_SOURCE_STATUS
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
execute_process(
	COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE rev-parse --short=10 HEAD
	OUTPUT_VARIABLE CHARACTER_PANEL_COMMONLIB_REVISION
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
execute_process(
	COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE status --porcelain
	OUTPUT_VARIABLE CHARACTER_PANEL_COMMONLIB_STATUS
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
if(NOT CHARACTER_PANEL_SOURCE_REVISION)
	set(CHARACTER_PANEL_SOURCE_REVISION "unrecorded")
endif()
if(NOT CHARACTER_PANEL_COMMONLIB_REVISION)
	set(CHARACTER_PANEL_COMMONLIB_REVISION "unrecorded")
endif()
if(CHARACTER_PANEL_SOURCE_STATUS STREQUAL "")
	set(CHARACTER_PANEL_SOURCE_SUFFIX "")
else()
	set(CHARACTER_PANEL_SOURCE_SUFFIX "+dirty")
endif()
if(CHARACTER_PANEL_COMMONLIB_STATUS STREQUAL "")
	set(CHARACTER_PANEL_COMMONLIB_SUFFIX "")
else()
	set(CHARACTER_PANEL_COMMONLIB_SUFFIX "+dirty")
endif()
string(TIMESTAMP CHARACTER_PANEL_CONFIGURE_STAMP "%Y%m%dT%H%M%SZ" UTC)
set(CHARACTER_PANEL_BUILD_IDENTITY
	"CharacterPanel-scenegraph-${CHARACTER_PANEL_BUILD_LABEL}-${CHARACTER_PANEL_SOURCE_REVISION}${CHARACTER_PANEL_SOURCE_SUFFIX}-cl${CHARACTER_PANEL_COMMONLIB_REVISION}${CHARACTER_PANEL_COMMONLIB_SUFFIX}-${CHARACTER_PANEL_CONFIGURE_STAMP}"
)

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
	${GENERATED_SHADER_HEADER}
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

		/Zc:inline
		/JMC-
		/Ob3
	)

	target_compile_definitions(
		CharacterPanel
		PRIVATE
		NOMINMAX
	)

	target_link_options(
		CharacterPanel
		PRIVATE
		/INCREMENTAL:NO
		/OPT:REF
		/OPT:ICF
		/DEBUG:FULL
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

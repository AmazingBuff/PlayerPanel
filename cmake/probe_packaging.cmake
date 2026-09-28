if(NOT DEFINED DLL_PATH OR NOT EXISTS "${DLL_PATH}")
    message(FATAL_ERROR "CharacterPanelProbe DLL was not found: ${DLL_PATH}")
endif()

if(NOT DEFINED PROJECT_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Probe packaging requires PROJECT_ROOT and OUTPUT_DIR")
endif()

set(stage_dir "${OUTPUT_DIR}/CharacterPanelProbe-stage")
set(package_path "${OUTPUT_DIR}/CharacterPanelProbe-${PROJECT_VERSION}.zip")
file(MAKE_DIRECTORY "${stage_dir}/SKSE/Plugins")
file(COPY_FILE "${DLL_PATH}" "${stage_dir}/SKSE/Plugins/CharacterPanelProbe.dll")
file(COPY_FILE "${PROJECT_ROOT}/tools/m0_probe/README.txt" "${stage_dir}/CharacterPanelProbe-README.txt")
file(SHA256 "${stage_dir}/SKSE/Plugins/CharacterPanelProbe.dll" dll_hash)
file(WRITE "${stage_dir}/build-manifest.txt"
    "artifact=CharacterPanelProbe.dll\n"
    "sha256=${dll_hash}\n"
    "build_identity=CharacterPanelProbe diagnostic target\n"
    "project_version=${PROJECT_VERSION}\n"
    "source_baseline=405dc1081b2a0bc7f84c2e9e60e56c636561bbcf\n"
    "contract=studio-m0-probe revision 1\n"
)
file(REMOVE "${package_path}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "cf" "${package_path}" --format=zip
        "SKSE/Plugins/CharacterPanelProbe.dll" "CharacterPanelProbe-README.txt" "build-manifest.txt"
    WORKING_DIRECTORY "${stage_dir}"
    RESULT_VARIABLE archive_result
)
if(NOT archive_result EQUAL 0)
    message(FATAL_ERROR "CharacterPanelProbe package creation failed: ${archive_result}")
endif()
message(STATUS "CharacterPanelProbe package: ${package_path}")

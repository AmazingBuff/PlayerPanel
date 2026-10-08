# Package the opt-in scene-graph-copy test build. The manifest records both
# binaries and the exact source baselines, so a result returned from another
# machine can be tied to the DLL that produced it.
if(NOT DEFINED DLL_PATH OR NOT EXISTS "${DLL_PATH}")
    message(FATAL_ERROR "CharacterPanel scene-copy DLL was not found: ${DLL_PATH}")
endif()

if(NOT DEFINED PDB_PATH OR NOT EXISTS "${PDB_PATH}")
    message(FATAL_ERROR "CharacterPanel scene-copy PDB was not found: ${PDB_PATH}")
endif()

if(NOT DEFINED PROJECT_ROOT OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Scene-copy packaging requires PROJECT_ROOT and OUTPUT_DIR")
endif()

if(NOT DEFINED SOURCE_BASELINE OR SOURCE_BASELINE STREQUAL "")
    set(SOURCE_BASELINE "unrecorded")
endif()

if(NOT DEFINED COMMONLIB_COMMIT OR COMMONLIB_COMMIT STREQUAL "")
    set(COMMONLIB_COMMIT "unrecorded")
endif()

set(stage_name "CharacterPanel-scopy-S0")
set(stage_dir "${OUTPUT_DIR}/${stage_name}")
set(package_path "${OUTPUT_DIR}/${stage_name}-${PROJECT_VERSION}.zip")

file(MAKE_DIRECTORY "${stage_dir}/SKSE/Plugins")
file(COPY_FILE "${DLL_PATH}" "${stage_dir}/SKSE/Plugins/CharacterPanel.dll")
file(COPY_FILE "${PDB_PATH}" "${stage_dir}/SKSE/Plugins/CharacterPanel.pdb")
file(COPY_FILE "${PROJECT_ROOT}/tools/scene_copy/README.txt" "${stage_dir}/CharacterPanel-scopy-README.txt")

file(SHA256 "${stage_dir}/SKSE/Plugins/CharacterPanel.dll" dll_hash)
file(SHA256 "${stage_dir}/SKSE/Plugins/CharacterPanel.pdb" pdb_hash)

string(TIMESTAMP build_timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)
# Each JSON SET call takes exactly one name/value pair and rewrites the variable.
set(manifest "{}")
string(JSON manifest SET "${manifest}" "experiment" "\"CharacterPanel-scopy-S0-2026-10-08\"")
string(JSON manifest SET "${manifest}" "artifact" "\"CharacterPanel.dll\"")
string(JSON manifest SET "${manifest}" "project_version" "\"${PROJECT_VERSION}\"")
string(JSON manifest SET "${manifest}" "configuration" "\"Release\"")
string(JSON manifest SET "${manifest}" "cmake_option_character_panel_scene_copy_experiment" "\"ON\"")
string(JSON manifest SET "${manifest}" "sha256_dll" "\"${dll_hash}\"")
string(JSON manifest SET "${manifest}" "sha256_pdb" "\"${pdb_hash}\"")
string(JSON manifest SET "${manifest}" "source_baseline" "\"${SOURCE_BASELINE}\"")
string(JSON manifest SET "${manifest}" "commonlibsse_commit" "\"${COMMONLIB_COMMIT}\"")
string(JSON manifest SET "${manifest}" "target_runtime" "\"Skyrim AE 1.6.1170 / SKSE 2.2.6\"")
string(JSON manifest SET "${manifest}" "build_timestamp_utc" "\"${build_timestamp}\"")
file(WRITE "${stage_dir}/build-manifest.json" "${manifest}\n")

file(REMOVE "${package_path}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "cf" "${package_path}" --format=zip
        "SKSE/Plugins/CharacterPanel.dll" "SKSE/Plugins/CharacterPanel.pdb"
        "CharacterPanel-scopy-README.txt" "build-manifest.json"
    WORKING_DIRECTORY "${stage_dir}"
    RESULT_VARIABLE archive_result
)
if(NOT archive_result EQUAL 0)
    message(FATAL_ERROR "Scene-copy package creation failed: ${archive_result}")
endif()

message(STATUS "Scene-copy package: ${package_path}")
message(STATUS "  dll sha256: ${dll_hash}")
message(STATUS "  source baseline: ${SOURCE_BASELINE}")
message(STATUS "  CommonLibSSE: ${COMMONLIB_COMMIT}")

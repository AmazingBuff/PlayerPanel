#pragma once

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <REX/W32/D3D11.h>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace std::literals;
namespace logger = SKSE::log;
#define DLLEXPORT __declspec(dllexport)

#include "plugin.h"

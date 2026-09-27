#pragma once

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <fmt/format.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using namespace std::literals;
namespace logger = SKSE::log;
#define DLLEXPORT __declspec(dllexport)

#include "plugin.h"

#pragma once
// Internal seam between pinstance.cpp (the tick caller) and dress.cpp (the
// definition): the one-shot build-window dressing of the clone from the
// player's worn set. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)

#include <RE/Skyrim.h>

namespace CharacterPanelProto
{
    void mirror_worn_equipment(RE::Actor* clone, RE::PlayerCharacter* player);
}

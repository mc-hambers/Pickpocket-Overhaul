// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
#include <cstdint>
namespace RE {class Actor;class PlayerCharacter;class TESObjectREFR;class InventoryEntryData;}
namespace PCRQuickPocketAPI {
// Main-thread-only native bridge. PCR owns all calculations and saved history.
struct V1 {
 std::uint32_t version;
 bool (*Ready)();
 void (*Begin)(RE::Actor*);
 void (*End)();
 std::int32_t (*Preview)(RE::Actor*,RE::InventoryEntryData*,std::int32_t);
 bool (*Attempt)(RE::PlayerCharacter*,RE::TESObjectREFR*,RE::InventoryEntryData*,std::int32_t,bool);
};
using Request=const V1*(*)(std::uint32_t);
// Export: PickpocketOverhaul_RequestQuickPocketAPI. No item transfer occurs here.
}

// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
#include <cstdint>
namespace LootCycleAPI {
inline constexpr std::uint32_t Changed=0x4A435931;
struct Event {std::uint32_t actor,cycle;};
struct V1 {std::uint32_t version=1; bool (*Query)(std::uint32_t actor,std::uint32_t* cycle);};
using Request=const V1*(*)(std::uint32_t);
}

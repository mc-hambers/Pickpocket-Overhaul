// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <cstddef>
namespace PO {
// A combined SE/AE ExtraDataList is a runtime-accessor facade. Its C++ sizeof can
// be smaller than the object constructed by the game. SE requires 0x18 bytes and
// AE 1.6.629+ requires 0x20. Do not enlarge the facade type: that would also change
// the compiled layout of references embedding it.
constexpr std::size_t extraDataCapacity(std::size_t facadeSize) {return std::max<std::size_t>(facadeSize,0x20);}
template<class Facade,class Allocate,class Construct>
Facade* allocateRuntimeExtraData(Allocate allocate,Construct construct) {
    void* storage=allocate(extraDataCapacity(sizeof(Facade)));
    if (!storage) return nullptr;
    construct(storage);
    return static_cast<Facade*>(storage);
}
}

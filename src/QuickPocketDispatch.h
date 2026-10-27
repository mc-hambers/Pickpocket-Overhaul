// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
#include "QuickPocketAPI.h"
namespace PCRQP {
using PreviewFn=std::int32_t(*)(RE::Actor*,RE::InventoryEntryData*);
using AttemptFn=bool(*)(RE::PlayerCharacter*,RE::TESObjectREFR*,RE::InventoryEntryData*,std::int32_t,bool);
inline int preview(const PCRQuickPocketAPI::V1* api,PreviewFn original,RE::Actor* victim,RE::InventoryEntryData* entry,int count){
 if(!api||!api->Ready())return original(victim,entry);
 const auto result=api->Preview(victim,entry,count);return result<0?original(victim,entry):result;
}
inline bool attempt(const PCRQuickPocketAPI::V1* api,AttemptFn original,RE::PlayerCharacter* player,RE::TESObjectREFR* victim,RE::InventoryEntryData* entry,int count,bool taking){
 // A reported failure is final. Never fall back and roll a second time.
 return api&&api->Ready()?api->Attempt(player,victim,entry,count,taking):original(player,victim,entry,count,taking);
}
}

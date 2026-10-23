// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace PO {
struct ContrabandRecord {std::string_view plugin;std::uint32_t localID;std::string_view editorID;};
// Vanilla records verified against Skyrim.esm and Dawnguard.esm. No empty
// skooma bottles, lockpicking potions or unique Black Star artifact.
inline constexpr std::array contrabandRecords{
 ContrabandRecord{"Skyrim.esm",0x57A7A,"Skooma"},
 ContrabandRecord{"Skyrim.esm",0x3F4BD,"WindhelmDoubleDistilledSkooma"},
 ContrabandRecord{"Skyrim.esm",0x57A7B,"FreeformAngasMillKordirsSkooma"},
 ContrabandRecord{"Skyrim.esm",0xAED90,"dunSleepingTreeCampSap"},
 ContrabandRecord{"Skyrim.esm",0xD8E3F,"MoonSugar"},
 ContrabandRecord{"Skyrim.esm",0x1016B3,"HumanFlesh"},
 ContrabandRecord{"Skyrim.esm",0xB18CD,"HumanHeart"},
 ContrabandRecord{"Skyrim.esm",0x2E500,"SoulGemBlack"},
 ContrabandRecord{"Skyrim.esm",0x2E504,"SoulGemBlackFilled"},
 ContrabandRecord{"Dawnguard.esm",0x1391D,"DLC1RedwaterDenSkooma"},
 ContrabandRecord{"Dawnguard.esm",0x18EF3,"DLC1BloodPotion"},
 ContrabandRecord{"Skyrim.esm",0xA,"Lockpick"}
};
inline bool contrabandEnabled(bool preference,bool externalProvider){return preference&&!externalProvider;}
// Exact normalized filename match, not a substring match against patch names.
inline bool contrabandProviderName(std::string_view name){
 constexpr std::string_view expected="contrabandconfiscation";std::size_t j=0;
 auto dot=name.find_last_of('.');if(dot==name.npos)return false;
 auto extension=name.substr(dot);if(extension!=".esp"&&extension!=".esl"&&extension!=".esm"&&extension!=".ESP"&&extension!=".ESL"&&extension!=".ESM")return false;
 for(char c:name.substr(0,dot)){if(c==' '||c=='_'||c=='-')continue;if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(j==expected.size()||c!=expected[j++])return false;}
 return j==expected.size();
}
}

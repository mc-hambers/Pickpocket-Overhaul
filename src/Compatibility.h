// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#pragma once
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>
namespace PO {
inline std::string_view trim(std::string_view text) {
    const auto begin=text.find_first_not_of(" \t\r\n");if(begin==text.npos)return {};
    return text.substr(begin,text.find_last_not_of(" \t\r\n")-begin+1);
}
inline bool loyaltyCounts(bool follower,bool guild,bool outlaw,bool soldier,bool sameSide,bool personalFriend,bool allied,bool sameOutlawGroup) {
    if(follower)return false;
    if(soldier)return personalFriend||sameSide;
    if(outlaw)return personalFriend||sameOutlawGroup;
    if(guild)return personalFriend||allied;
    return true;
}
struct MappedForm {std::string plugin;std::uint32_t localID;};
inline std::optional<MappedForm> mappedForm(std::string_view text) {
    text=trim(text);const auto separator=text.find('|');if(separator==text.npos)return {};
    const auto plugin=trim(text.substr(0,separator));auto id=trim(text.substr(separator+1));
    if(id.starts_with("0x")||id.starts_with("0X"))id.remove_prefix(2);
    std::uint32_t value=0;const auto result=std::from_chars(id.data(),id.data()+id.size(),value,16);
    if(plugin.empty()||id.empty()||result.ec!=std::errc{}||result.ptr!=id.data()+id.size()||!value||value>0xFFFFFF)return {};
    if(!(plugin.ends_with(".esp")||plugin.ends_with(".esm")||plugin.ends_with(".esl")))return {};
    return MappedForm{std::string(plugin),value};
}
}

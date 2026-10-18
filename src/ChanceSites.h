// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
namespace PO::Hooks {
inline std::int64_t destination(std::span<const std::uint8_t> bytes,std::size_t call,std::uint64_t address) {
    if(call>bytes.size()||bytes.size()-call<5||bytes[call]!=0xE8)throw std::runtime_error("Expected near CALL");
    std::int32_t offset;std::memcpy(&offset,bytes.data()+call+1,4);
    return static_cast<std::int64_t>(address)+5+offset;
}
struct Site {std::uint32_t rva;std::size_t fileOffset;};
inline std::vector<Site> discover(std::span<const std::uint8_t> file,std::uint64_t targetRva) {
    auto u16=[&](std::size_t p){if(p>file.size()||file.size()-p<2)throw std::runtime_error("Truncated PE");std::uint16_t v;std::memcpy(&v,file.data()+p,2);return v;};
    auto u32=[&](std::size_t p){if(p>file.size()||file.size()-p<4)throw std::runtime_error("Truncated PE");std::uint32_t v;std::memcpy(&v,file.data()+p,4);return v;};
    if(u16(0)!=0x5A4D)throw std::runtime_error("Invalid DOS header");
    const std::size_t pe=u32(0x3C);
    if(u32(pe)!=0x4550||u16(pe+4)!=0x8664||u16(pe+24)!=0x20B)throw std::runtime_error("Expected x64 PE");
    const auto count=u16(pe+6);if(count==0||count>96)throw std::runtime_error("Invalid section count");
    const auto sections=pe+24+u16(pe+20);std::vector<Site> sites;
    for(unsigned n=0;n<count;++n){const auto header=sections+n*40;const auto rva=u32(header+12),size=u32(header+16),raw=u32(header+20),flags=u32(header+36);
        if(!(flags&0x20000000))continue;
        if(raw>file.size()||size>file.size()-raw)throw std::runtime_error("Truncated executable section");
        for(std::size_t i=8;i+13<=size;++i)if(file[raw+i]==0xE8&&destination(file,raw+i,std::uint64_t(rva)+i)==static_cast<std::int64_t>(targetRva)){
            if(std::uint64_t(rva)+i>UINT32_MAX)throw std::runtime_error("Invalid RVA");
            sites.push_back({static_cast<std::uint32_t>(rva+i),raw+i});
        }
    }
    return sites;
}
inline bool contextMatches(std::span<const std::uint8_t> file,const Site& site,std::span<const std::uint8_t> live) {
    const auto p=site.fileOffset;
    if(p<8||p>file.size()||file.size()-p<13||live.size()!=21||live[8]!=0xE8)return false;
    // Only the CALL's displacement may differ. Never overwrite a replaced instruction.
    return std::memcmp(file.data()+p-8,live.data(),9)==0&&std::memcmp(file.data()+p+5,live.data()+13,8)==0;
}
}

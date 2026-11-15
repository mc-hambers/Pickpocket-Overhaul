// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#pragma once
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace PO::Hooks {
template<class Copy> inline void overlayExecutable(std::vector<std::uint8_t>& file,Copy copy) {
    auto u16=[&](std::size_t p){if(p>file.size()||file.size()-p<2)throw std::runtime_error("Truncated PE");std::uint16_t v;std::memcpy(&v,file.data()+p,2);return v;};
    auto u32=[&](std::size_t p){if(p>file.size()||file.size()-p<4)throw std::runtime_error("Truncated PE");std::uint32_t v;std::memcpy(&v,file.data()+p,4);return v;};
    if(u16(0)!=0x5A4D)throw std::runtime_error("Invalid DOS header");
    const auto pe=u32(0x3c);
    if(u32(pe)!=0x4550||u16(pe+4)!=0x8664||u16(pe+24)!=0x20b)throw std::runtime_error("Expected x64 PE");
    const auto count=u16(pe+6),optional=u16(pe+20);const auto imageSize=u32(pe+24+56);
    if(!count||count>96||optional<60)throw std::runtime_error("Invalid executable headers");
    for(unsigned n=0;n<count;++n) {
        const auto h=pe+24+optional+n*40;const auto flags=u32(h+36);
        if(!(flags&0x20000000)||std::memcmp(file.data()+h,".bind",5)==0)continue;
        const auto rva=u32(h+12),rawSize=u32(h+16),raw=u32(h+20),virtualSize=u32(h+8);
        if(raw>file.size()||rawSize>file.size()-raw||rva>imageSize||rawSize>imageSize-rva||virtualSize>imageSize-rva)throw std::runtime_error("Executable section out of bounds");
        copy(rva,std::span<std::uint8_t>(file.data()+raw,rawSize));
    }
}
inline std::int64_t destination(std::span<const std::uint8_t> bytes,std::size_t call,std::uint64_t address) {
    if(call>bytes.size()||bytes.size()-call<5||bytes[call]!=0xE8)throw std::runtime_error("Expected near CALL");
    std::int32_t offset;std::memcpy(&offset,bytes.data()+call+1,4);
    return static_cast<std::int64_t>(address)+5+offset;
}
struct Site {std::uint32_t rva;std::size_t fileOffset;};
inline Site equippedTransferSite(std::span<const std::uint8_t> file,const Site& attempt) {
    // Only the taking branch after the validated successful-attempt caller.
    // MSVC member ABI: RCX=source, RDX=result handle, R8=item, R9=count.
    // Require the stack result address, source load and RemoveItem slot 0x56.
    std::vector<Site> matches;
    if(attempt.fileOffset>file.size()||file.size()-attempt.fileOffset<5)throw std::runtime_error("Invalid attempt boundary");
    const auto end=std::min(file.size(),attempt.fileOffset+0x280);
    for(auto p=attempt.fileOffset+16;p+14<=end;++p) {
        if(file[p]==0xFF&&(file[p+1]>=0x90&&file[p+1]<=0x97)&&file[p+1]!=0x94&&
           file[p+2]==0xB0&&file[p+3]==2&&file[p+4]==0&&file[p+5]==0&&
           file[p-8]==0x48&&file[p-7]==0x8D&&file[p-6]==0x54&&file[p-5]==0x24&&
           file[p-3]==0x48&&file[p-2]==0x8B&&(file[p-1]&0xF8)==0xC8)
            matches.push_back({static_cast<std::uint32_t>(attempt.rva+p-attempt.fileOffset),p});
    }
    if(matches.size()!=1)throw std::runtime_error("Expected one equipped transfer member call after theft roll");
    return matches.front();
}
inline Site karmaSite(std::span<const std::uint8_t> file,std::uint32_t attemptRva) {
    auto u32=[&](std::size_t p){if(p>file.size()||file.size()-p<4)throw std::runtime_error("Truncated PE");std::uint32_t v;std::memcpy(&v,file.data()+p,4);return v;};
    auto u16=[&](std::size_t p){if(p>file.size()||file.size()-p<2)throw std::runtime_error("Truncated PE");std::uint16_t v;std::memcpy(&v,file.data()+p,2);return v;};
    if(u16(0)!=0x5A4D)throw std::runtime_error("Invalid PE");
    const auto pe=u32(0x3c);if(u32(pe)!=0x4550||u16(pe+4)!=0x8664||u16(pe+24)!=0x20b)throw std::runtime_error("Expected x64 PE");
    const auto sections=pe+24+u16(pe+20);const auto count=u16(pe+6);
    if(count==0||count>96)throw std::runtime_error("Invalid section count");
    std::vector<Site> matches;
    for(unsigned n=0;n<count;++n){const auto h=sections+n*40;const auto rva=u32(h+12),size=u32(h+16),raw=u32(h+20);
        if(!(u32(h+36)&0x20000000)||attemptRva<rva||attemptRva-rva>=size)continue;
        if(raw>file.size()||size>file.size()-raw)throw std::runtime_error("Truncated section");
        const std::size_t start=std::size_t(raw)+(attemptRva-rva),end=std::min<std::size_t>(std::size_t(raw)+size,start+0x300);
        // mov edx,100; lea rcx,[rng]; call uniform; cmp eax,ebx; jge fail.
        // No offsets or fixed executable addresses: unsupported layouts fail closed.
        for(auto p=start;p+21<=end;++p)if(file[p]==0xBA&&u32(p+1)==100&&file[p+5]==0x48&&file[p+6]==0x8D&&file[p+7]==0x0D&&file[p+12]==0xE8&&file[p+17]==0x3B&&file[p+18]==0xC3&&file[p+19]==0x7D)
            matches.push_back({std::uint32_t(attemptRva+(p-start)+12),p+12});
    }
    if(matches.size()!=1)throw std::runtime_error("Expected one verified percentile RNG call inside theft attempt");
    return matches.front();
}
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
inline std::vector<Site> chanceSites(std::span<const std::uint8_t> file,std::uint64_t targetRva) {
    // Identify each caller by its argument setup and result handling, not by
    // its current CALL destination. SKSE plugins routinely redirect E8 calls.
    // These contexts are shared by the independently audited 1170/1.7.104
    // layouts; offsets, image bases and CALL displacements are not assumed.
    constexpr std::uint8_t rollBefore[]={0x4c,0x89,0x6c,0x24,0x38,0x40,0x88,0x74,0x24,0x30,0x48,0x89,0x6c,0x24,0x28,0x48,0x89,0x5c,0x24,0x20,0x0f,0x28,0xdf,0x45,0x8b,0xc7,0x0f,0x28,0xce};
    constexpr std::uint8_t menuBefore[]={0x4c,0x89,0x7c,0x24,0x38,0x44,0x88,0x74,0x24,0x30,0x4c,0x89,0x64,0x24,0x28,0x48,0x89,0x5c,0x24,0x20,0x0f,0x28,0xdf,0x44,0x8b,0x45,0x6f,0x0f,0x28,0xce};
    constexpr std::uint8_t rollAfter[]={0x8b,0xd8,0x8b,0x0d};
    constexpr std::uint8_t rollTail[]={0x65,0x48,0x8b,0x04,0x25,0x58,0x00,0x00,0x00,0x41,0xb8,0x08,0x2a,0x00,0x00};
    constexpr std::uint8_t menuAfter[]={0x83,0xf8,0x64,0x7e,0x0a,0xb8,0x64,0x00,0x00,0x00,0x45,0x33,0xf6,0xeb,0x0c,0x85,0xc0,0x41,0xbe,0x00,0x00,0x00,0x00,0x41,0x0f,0x48,0xc6};
    auto u16=[&](std::size_t p){if(p>file.size()||file.size()-p<2)throw std::runtime_error("Truncated PE");std::uint16_t v;std::memcpy(&v,file.data()+p,2);return v;};
    auto u32=[&](std::size_t p){if(p>file.size()||file.size()-p<4)throw std::runtime_error("Truncated PE");std::uint32_t v;std::memcpy(&v,file.data()+p,4);return v;};
    if(u16(0)!=0x5a4d)throw std::runtime_error("Invalid DOS header");
    const auto pe=u32(0x3c);if(u32(pe)!=0x4550||u16(pe+4)!=0x8664||u16(pe+24)!=0x20b)throw std::runtime_error("Expected x64 PE");
    const auto count=u16(pe+6);const auto table=pe+24+u16(pe+20);
    if(!count||count>96)throw std::runtime_error("Invalid section count");
    std::vector<Site> roll,menu;
    for(unsigned n=0;n<count;++n){
        const auto h=table+n*40;const auto rva=u32(h+12),size=u32(h+16),raw=u32(h+20);
        if(!(u32(h+36)&0x20000000)||std::memcmp(file.data()+h,".bind",5)==0)continue;
        if(raw>file.size()||size>file.size()-raw)throw std::runtime_error("Truncated executable section");
        for(std::size_t i=sizeof(menuBefore);i+5+sizeof(menuAfter)<=size;++i){
            const auto p=raw+i;
            if(file[p]!=0xe8)continue;
            if(std::memcmp(file.data()+p-sizeof(rollBefore),rollBefore,sizeof(rollBefore))==0&&
               std::memcmp(file.data()+p+5,rollAfter,sizeof(rollAfter))==0&&
               std::memcmp(file.data()+p+13,rollTail,sizeof(rollTail))==0)roll.push_back({static_cast<std::uint32_t>(rva+i),p});
            if(std::memcmp(file.data()+p-sizeof(menuBefore),menuBefore,sizeof(menuBefore))==0&&
               std::memcmp(file.data()+p+5,menuAfter,sizeof(menuAfter))==0)menu.push_back({static_cast<std::uint32_t>(rva+i),p});
        }
    }
    if(roll.size()==1&&menu.size()==1&&roll[0].rva!=menu[0].rva)return {roll[0],menu[0]};
    if(roll.size()>1||menu.size()>1)throw std::runtime_error("Ambiguous chance caller signatures; no hooks installed");
    // Retain the old route for other unmodified supported runtime layouts.
    auto direct=discover(file,targetRva);
    if(direct.size()==2)return direct;
    throw std::runtime_error("Chance caller contexts unavailable: theft="+std::to_string(roll.size())+", preview="+std::to_string(menu.size())+", original destinations="+std::to_string(direct.size())+"; no unsafe hooks installed");
}
inline bool contextMatches(std::span<const std::uint8_t> file,const Site& site,std::span<const std::uint8_t> live) {
    const auto p=site.fileOffset;
    if(p<8||p>file.size()||file.size()-p<13||live.size()!=21||live[8]!=0xE8)return false;
    // Only the CALL's displacement may differ. Never overwrite a replaced instruction.
    return std::memcmp(file.data()+p-8,live.data(),9)==0&&std::memcmp(file.data()+p+5,live.data()+13,8)==0;
}
}


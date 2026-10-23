// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace PO {
struct BadLuckProtection {
    std::uint32_t credit=0;
    unsigned effective(unsigned base) const {
        if(base==0||base>=100)return std::min(base,100u);
        return std::min(99u,base+(credit>150?(credit-150)/10:0));
    }
    void outcome(unsigned base,bool success) {
        if(success)credit=0;
        else if(base>0&&base<100)credit=std::min(1000u,credit+base);
    }
    unsigned roll(unsigned base,unsigned ordinary) const {
        // Preserve native wins. Convert only the extra protected interval into
        // a win against the engine's unchanged comparison with the base chance.
        return base>0&&ordinary>=base&&ordinary<effective(base)?base-1:ordinary;
    }
    bool valid() const {return credit<=1000;}
};
// Sampling without replacement keeps the base percentile distribution while
// reducing streaks. One shared deck belongs to the loaded character, not an NPC.
struct KarmaRolls {
    std::uint64_t seed=0;
    std::uint32_t cursor=100;
    std::array<std::uint8_t,100> bag{};
    KarmaRolls() {for(unsigned i=0;i<100;++i)bag[i]=static_cast<std::uint8_t>(i);}
    std::uint64_t next() {
        auto z=(seed+=0x9e3779b97f4a7c15ULL);
        z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
        z=(z^(z>>27))*0x94d049bb133111ebULL;
        return z^(z>>31);
    }
    unsigned uniform(unsigned n) {
        const auto threshold=(std::uint64_t(0)-n)%n;
        std::uint64_t x;do{x=next();}while(x<threshold);
        return unsigned(x%n);
    }
    unsigned draw() {
        if(cursor>=100){for(unsigned i=99;i>0;--i)std::swap(bag[i],bag[uniform(i+1)]);cursor=0;}
        return bag[cursor++];
    }
    bool valid() const {
        if(cursor>100)return false;
        std::array<bool,100> seen{};
        for(auto v:bag){if(v>=100||seen[v])return false;seen[v]=true;}
        return true;
    }
};
}

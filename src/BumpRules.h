// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <cmath>
namespace PO {
struct BumpRules {
    double nextBonusDay=0,blockedUntilDay=0,recognizedUntilDay=0,bonusUntil=0;
    bool allowed(double day,float sneak,float pickpocket) const {
        return std::isfinite(day)&&day>=blockedUntilDay&&day>=recognizedUntilDay&&sneak<75&&pickpocket<75;
    }
    void failed(double day) {blockedUntilDay=std::max(blockedUntilDay,day+1);bonusUntil=0;}
    void witnessed(double day) {recognizedUntilDay=std::max(recognizedUntilDay,day+1.0/24);bonusUntil=0;}
    void contact(double day,double until,bool distinct,bool first,bool unrecognized,bool eligible) {
        if(!distinct){if(bonusUntil>0)bonusUntil=std::max(bonusUntil,until);return;}
        bonusUntil=0;
        if(eligible&&first&&unrecognized&&day>=nextBonusDay&&until>0){bonusUntil=until;nextBonusDay=day+1.0/24;}
    }
    bool valid() const {return std::isfinite(nextBonusDay)&&std::isfinite(blockedUntilDay)&&std::isfinite(recognizedUntilDay)&&std::isfinite(bonusUntil)&&bonusUntil>=0;}
};
inline float perkMaximum(bool unlocked) {return unlocked?99.f:89.f;}
}

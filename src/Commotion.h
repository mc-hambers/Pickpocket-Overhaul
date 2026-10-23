// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace PO {
enum class Noise : unsigned { Combat, Hit, Meal, Argument, Explosion, Panic, Count };
struct Commotion {
    struct Signal {float strength=0;double until=0;};
    std::array<Signal,static_cast<unsigned>(Noise::Count)> signals{};
    void add(Noise kind,float strength,double now,double duration) {
        if(!std::isfinite(strength)||!std::isfinite(now)||!std::isfinite(duration)||strength<=0||duration<=0)return;
        auto& signal=signals[static_cast<unsigned>(kind)];
        signal.strength=signal.until>now?std::max(signal.strength,strength):strength;
        signal.until=std::max(signal.until,now+duration);
    }
    float value(double now) const {
        float highest=0,total=0;
        for(const auto& signal:signals)if(signal.until>now){highest=std::max(highest,signal.strength);total+=signal.strength;}
        return std::min(24.f,highest+std::min(4.f,(total-highest)*.25f));
    }
    bool expired(double now)const {for(const auto& s:signals)if(s.until>now)return false;return true;}
};
inline float noiseAtDistance(float distance,float radius,float strength,bool clear) {
    if(!clear||!std::isfinite(distance)||!std::isfinite(radius)||radius<=0||distance<0||distance>=radius)return 0;
    return strength*(1-distance/radius);
}
inline bool recoveryWalkStalled(bool claimed,bool finished,bool pathing,double elapsed) {
    return claimed&&!finished&&!pathing&&elapsed>=12;
}
}

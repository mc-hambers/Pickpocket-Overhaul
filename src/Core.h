// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. NO WARRANTY; see LICENSE.txt.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace PO {
struct Config {
    float minimum=50,maximum=98,distance=.65f,weight=8,value=3,attention=.6f,witness=1;
    float tagDays=1,recognitionDays=3;
};
inline float finite(float n,float fallback=0) {return std::isfinite(n)?n:fallback;}
inline void normalize(Config& c) {
    c.minimum=std::clamp(finite(c.minimum,50),0.f,100.f);
    c.maximum=std::clamp(finite(c.maximum,98),c.minimum,100.f);
    c.distance=std::clamp(finite(c.distance,.65f),.1f,1.f);
    c.weight=std::clamp(finite(c.weight,8),0.f,30.f);
    c.value=std::clamp(finite(c.value,3),0.f,20.f);
    c.attention=std::clamp(finite(c.attention,.6f),0.f,2.f);
    c.witness=std::clamp(finite(c.witness,1),0.f,3.f);
    c.tagDays=std::clamp(finite(c.tagDays,1),.1f,30.f);
    c.recognitionDays=std::clamp(finite(c.recognitionDays,3),.1f,90.f);
}
inline float cap(float skill,const Config& c) {
    return std::clamp(c.minimum+std::floor(std::clamp(finite(skill),0.f,100.f)*(c.maximum-c.minimum)/100.f),0.f,100.f);
}
struct Situation {
    float attention=25,misdirection=0,witnessPenalty=0;
    bool facing=false,close=false,detected=false,sleeping=false,sitting=false,talking=false,focusedElsewhere=false;
    unsigned crowd=0,alerted=0;
};
inline float attention(const Situation& f) {
    return std::clamp(f.attention+(f.facing?22:0)+(f.close?10:0)+(f.detected?18:0)+std::min(f.alerted,3u)*6-
        (f.sleeping?60:0)-(f.sitting?8:0)-(f.talking&&!f.facing?14:0)-(f.focusedElsewhere?12:0),0.f,100.f);
}
inline float handChance(float weight,unsigned value,const Config& c) {
    return std::clamp(100.f-c.weight*std::sqrt(std::max(0.f,finite(weight)))-c.value*std::log1p(float(value)),0.f,100.f);
}
inline int finalChance(float hand,float skill,const Situation& f,const Config& c) {
    const float distraction=(std::clamp(f.misdirection,0.f,30.f)+std::min(f.crowd,6u)*1.5f)*(1+std::clamp(finite(skill),0.f,100.f)*.0075f);
    const float raw=finite(hand)-attention(f)*c.attention+distraction-std::max(0.f,f.witnessPenalty)*c.witness;
    return int(std::floor(std::clamp(finite(raw),0.f,cap(skill,c))));
}
struct AttentionState {
    double last=0,bumpUntil=0,lastBump=-100,commotionUntil=0,observedUntil=0,failedUntil=0,interactionUntil=0;
    float suspicion=0;unsigned bumps=0;
};
inline void decay(AttentionState& s,double now) {
    if (!std::isfinite(now)) return;
    if (now<s.last) {s={};s.last=now;return;}
    s.suspicion=std::max(0.f,s.suspicion-float(now-s.last)*.15f);s.last=now;
    if (now-s.lastBump>60) s.bumps=0;
}
inline void bump(AttentionState& s,double now) {
    decay(s,now);if(now-s.lastBump<1) return;
    s.lastBump=now;
    if (++s.bumps<=2) s.bumpUntil=now+4;
    else {s.bumpUntil=0;s.suspicion=std::min(60.f,s.suspicion+15);}
}
inline bool witnessCounts(bool follower,bool guild,bool outlaw,bool friendOfVictim,bool sameFaction) {
    if(follower) return false;
    if(guild) return friendOfVictim;
    if(outlaw) return sameFaction;
    return true;
}
inline double memoryDays(unsigned value,float scale) {return double(scale)*(1+std::log1p(double(value))/4);}
struct Stolen {
    std::uint32_t victim=0,form=0,owner=0,container=0,uidBase=0,value=0;
    std::uint16_t uid=0;bool reported=false;
    double noticed=0,expires=0,recognizesUntil=0;
};
}

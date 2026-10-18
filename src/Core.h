// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. NO WARRANTY; see LICENSE.txt.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace PO {
struct Config {
    float awareness=50,distraction=20,distance=.65f,weight=15,value=10,profile=5,witness=1;
    float tagDays=1,recognitionDays=3,maximum=98;
};
inline float finite(float n,float fallback=0) {return std::isfinite(n)?n:fallback;}
inline void normalize(Config& c) {
    c.awareness=std::clamp(finite(c.awareness,50),0.f,100.f);
    c.distraction=std::clamp(finite(c.distraction,20),0.f,100.f);
    c.maximum=std::clamp(finite(c.maximum,98),0.f,100.f);
    c.distance=std::clamp(finite(c.distance,.65f),.1f,1.f);
    c.weight=std::clamp(finite(c.weight,15),0.f,100.f);
    c.value=std::clamp(finite(c.value,10),0.f,100.f);
    c.profile=std::clamp(finite(c.profile,5),0.f,100.f);
    c.witness=std::clamp(finite(c.witness,1),0.f,3.f);
    c.tagDays=std::clamp(finite(c.tagDays,1),.1f,30.f);
    c.recognitionDays=std::clamp(finite(c.recognitionDays,3),.1f,90.f);
}
struct Situation {
    float attention=25,misdirection=0,witnessPenalty=0,weight=0,profileRisk=0,detection=0;
    unsigned value=0;
    bool facing=false,close=false,detected=false,sleeping=false,sitting=false,talking=false,speakingToPlayer=false,focusedElsewhere=false;
    unsigned crowd=0,alerted=0;
};
inline float attention(const Situation& f) {
    if(f.sleeping)return 0;
    return std::clamp(finite(f.attention)+(f.facing?22:0)+(f.close?10:0)+
        std::max(f.detected?18.f:0.f,std::clamp(finite(f.detection),0.f,1.f)*25)+std::min(f.alerted,3u)*6-
        (f.sitting?8:0)-(f.speakingToPlayer?8:0)-(f.talking&&!f.facing?14:0)-(f.focusedElsewhere?12:0),0.f,100.f);
}
inline float distraction(const Situation& f) {
    return std::clamp((finite(f.misdirection)+std::min(f.crowd,6u)*1.5f)/30.f,0.f,1.f);
}
inline float weightBurden(float weight) {return std::clamp(std::log1p(std::max(0.f,finite(weight)))/std::log(31.f),0.f,1.f);}
inline float valueBurden(unsigned value) {return std::clamp(std::log1p(float(value))/std::log(2001.f),0.f,1.f);}
inline float cap(const Situation& f,const Config& c) {
    const float d=distraction(f);
    // A skill/perk bonus cannot rescue an actively watched, undistracted theft.
    if(f.facing&&f.detected&&!f.sleeping&&!f.focusedElsewhere&&d<.7f)return 0;
    const float eyes=std::clamp((attention(f)+std::max(0.f,finite(f.witnessPenalty))*c.witness)/100.f,0.f,1.f);
    return std::clamp(c.maximum-c.awareness*eyes+c.distraction*d-c.weight*weightBurden(f.weight)-
        c.value*valueBurden(f.value)-c.profile*std::clamp(finite(f.profileRisk),-1.f,1.f),0.f,c.maximum);
}
inline float handChance(float weight,unsigned value,const Config&,float skill) {
    const float trained=std::clamp(finite(skill),0.f,100.f)/100;
    // Skill reduces handling/security losses, not the environmental ceiling.
    return std::clamp(5+95*trained-20*weightBurden(weight)*(1-.65f*trained)-
        12*valueBurden(value)*(1-.9f*trained),0.f,100.f);
}
inline int finalChance(float hand,float skill,const Situation& f,const Config& c) {
    const float trained=std::clamp(finite(skill),0.f,100.f)/100;
    const float raw=finite(hand)+c.distraction*distraction(f)*(.5f+trained);
    return int(std::floor(std::clamp(finite(raw),0.f,cap(f,c))));
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
// Transient observations are deliberately not saved: loading/re-entering range
// establishes a baseline rather than replaying an old engine bump.
struct BumpObservation {
    float stamp=0;int type=-1;bool initialized=false;double contactUntil=-1;
};
inline bool nativeBump(BumpObservation& observation,float stamp,int type,bool playerContact,double now) {
    if(!std::isfinite(stamp)||!std::isfinite(now)){observation={};return false;}
    if(playerContact)observation.contactUntil=now+.6;
    const bool changed=observation.initialized&&(stamp!=observation.stamp||type!=observation.type);
    observation.stamp=stamp;observation.type=type;
    if(!observation.initialized){observation.initialized=true;return false;}
    return changed&&type==0&&now<=observation.contactUntil;
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

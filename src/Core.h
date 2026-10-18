// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. NO WARRANTY; see LICENSE.txt.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace PO {
struct Config {
    float awareness=50,distraction=20,distance=.65f,weight=15,value=10,profile=7.5f,witness=1;
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
    c.profile=std::clamp(finite(c.profile,7.5f),0.f,100.f);
    c.witness=std::clamp(finite(c.witness,1),0.f,3.f);
    c.tagDays=std::clamp(finite(c.tagDays,1),.1f,30.f);
    c.recognitionDays=std::clamp(finite(c.recognitionDays,3),.1f,90.f);
}
struct Situation {
    float attention=25,misdirection=0,witnessPenalty=0,weight=0,profileRisk=0,detection=0,ageRelief=0,hour=-1,weather=0;
    unsigned value=0;
    bool facing=false,close=false,detected=false,sleeping=false,sitting=false,talking=false,speakingToPlayer=false,focusedElsewhere=false,bumpDistracted=false,guard=false,outdoors=false;
    unsigned crowd=0,alerted=0;
};
inline float timeAwareness(float hour,bool guard) {
    if(guard||!std::isfinite(hour)||hour<0)return 0;
    const float h=std::fmod(hour,24.f),pi=3.14159265359f;
    if(h>=6&&h<=18)return 5.f*std::sin((h-6)*pi/12);
    return -10.f*std::sin(std::fmod(h+6,24.f)*pi/12);
}
inline float weatherDistraction(const Situation& f) {return f.outdoors?std::clamp(finite(f.weather),0.f,12.f):0.f;}
inline float attention(const Situation& f) {
    if(f.sleeping)return 0;
    // A genuine light-bump window suppresses immediate contact/turn reactions,
    // while prior suspicion, failures, interactions and witnesses remain.
    const float reaction=f.bumpDistracted?0.f:(f.facing?22.f:0.f)+(f.close?10.f:0.f)+
        std::max(f.detected?18.f:0.f,std::clamp(finite(f.detection),0.f,1.f)*25);
    return std::clamp(finite(f.attention)+timeAwareness(f.hour,f.guard)+reaction+std::min(f.alerted,3u)*6-
        (f.sitting?8:0)-(f.speakingToPlayer?8:0)-(f.talking&&!f.facing?14:0)-(f.focusedElsewhere?12:0),0.f,100.f);
}
inline float distraction(const Situation& f) {
    return std::clamp((finite(f.misdirection)+weatherDistraction(f)+std::min(f.crowd,6u)*1.5f)/30.f,0.f,1.f);
}
inline float weightBurden(float weight) {return std::clamp(std::log1p(std::max(0.f,finite(weight)))/std::log(31.f),0.f,1.f);}
inline float valueBurden(unsigned value) {return std::clamp(std::log1p(float(value))/std::log(2001.f),0.f,1.f);}
inline float awarenessLoss(const Situation& f,const Config& c) {
    const float eyes=std::clamp((attention(f)+std::max(0.f,finite(f.witnessPenalty))*c.witness)/100.f,0.f,1.f);
    return c.awareness*eyes;
}
inline bool elderRecord(std::uint32_t race,std::uint32_t baseRace,std::uint32_t voice) {
    const auto elderRace=[](std::uint32_t id){return id==0x67CD8||id==0xA82BA;};
    return elderRace(race)||elderRace(baseRace)||voice==0x13AE2||voice==0x13AE1||voice==0x13AD7||voice==0x13AD6;
}
inline float ageBenefit(const Situation& f,const Config& c) {return c.profile*std::clamp(finite(f.ageRelief),0.f,1.f);}
inline float witnessLoss(const Situation& f,const Config& c) {
    return c.awareness*std::clamp(std::max(0.f,finite(f.witnessPenalty))*c.witness/100.f,0.f,1.f);
}
inline float skillCapRelief(const Situation& f,const Config& c,float skill) {
    const float trained=std::clamp(finite(skill),0.f,100.f)/100;
    const float lightness=1.f-weightBurden(f.weight);
    return trained*lightness*std::min(20.f,12.f+.8f*c.value*valueBurden(f.value)+.5f*c.weight*weightBurden(f.weight));
}
inline float cap(const Situation& f,const Config& c,float skill=0) {
    const float d=distraction(f);
    // A skill/perk bonus cannot rescue an actively watched, undistracted theft.
    if(f.facing&&f.detected&&!f.sleeping&&!f.focusedElsewhere&&d<.7f)return 0;
    return std::clamp(c.maximum-awarenessLoss(f,c)+c.distraction*d+skillCapRelief(f,c,skill)-c.weight*weightBurden(f.weight)-
        c.value*valueBurden(f.value)-c.profile*std::clamp(finite(f.profileRisk)-std::clamp(finite(f.ageRelief),0.f,1.f),-1.f,1.f),0.f,c.maximum);
}
inline float handChance(float weight,unsigned value,const Config&,float /*skill*/) {
    // Perk entry points modify execution. Skill controls the situational cap
    // and use of distractions rather than this neutral item-handling baseline.
    return std::clamp(80.f-20.f*weightBurden(weight)-12.f*valueBurden(value),0.f,100.f);
}
inline float sleepRelief(const Situation& f,const Config& c) {
    if(!f.sleeping)return 0;
    auto awake=f;awake.sleeping=false;
    // Credit only the awareness loss actually removed by sleep. Witnesses stay
    // in both evaluations and cannot create or increase this relief.
    return std::max(0.f,awarenessLoss(awake,c)-awarenessLoss(f,c));
}
inline int finalChance(float hand,float skill,const Situation& f,const Config& c) {
    const float trained=std::clamp(finite(skill),0.f,100.f)/100;
    const float raw=finite(hand)+sleepRelief(f,c)+ageBenefit(f,c)-witnessLoss(f,c)+c.distraction*distraction(f)*(.5f+trained);
    return int(std::floor(std::clamp(finite(raw),0.f,cap(f,c,skill))));
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
    if (++s.bumps<=2) s.bumpUntil=now+6;
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

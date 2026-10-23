// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. NO WARRANTY; see LICENSE.txt.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
namespace PO {
struct Config {
    float awareness=50,distraction=20,distance=.45f,weight=15,value=10,profile=7.5f,witness=1;
    float tagDays=3,recognitionDays=1,maximum=97,pointSkew=1;
};
inline bool editableOption(int i) {return i==2||i==6||i==7||i==8||i==9||i==10;}
inline float finite(float n,float fallback=0) {return std::isfinite(n)?n:fallback;}
inline void normalize(Config& c) {
    c.pointSkew=std::clamp(finite(c.pointSkew,1),0.f,3.f);
    c.awareness=std::clamp(finite(c.awareness,50),0.f,100.f);
    c.distraction=std::clamp(finite(c.distraction,20),0.f,100.f);
    c.maximum=std::clamp(finite(c.maximum,97),0.f,100.f);
    c.distance=std::clamp(finite(c.distance,.45f),.1f,1.f);
    c.weight=std::clamp(finite(c.weight,15),0.f,100.f);
    c.value=std::clamp(finite(c.value,10),0.f,100.f);
    c.profile=std::clamp(finite(c.profile,7.5f),0.f,100.f);
    c.witness=std::clamp(finite(c.witness,1),0.f,3.f);
    c.tagDays=std::clamp(finite(c.tagDays,3),.1f,30.f);
    c.recognitionDays=std::clamp(finite(c.recognitionDays,1),.1f,90.f);
}
struct Situation {
    bool ignorePersonalAwareness=false;
    float attention=25,misdirection=0,witnessPenalty=0,weight=0,profileRisk=0,detection=0,ageRelief=0,hour=-1,weather=0,lingeringReaction=0,bumpEffect=1;
    unsigned value=0,successfulStacks=0;
    bool fleeing=false,facing=false,close=false,detected=false,sleeping=false,sitting=false,talking=false,speakingToPlayer=false,focusedElsewhere=false,bumpDistracted=false,bumpBonus=false,guard=false,outdoors=false,failed=false;
    unsigned crowd=0,alerted=0;
};
inline float timeAwareness(float hour,bool guard) {
    if(guard||!std::isfinite(hour)||hour<0)return 0;
    const float h=std::fmod(hour,24.f),pi=3.14159265359f;
    if(h>=6&&h<=18)return 5.f*std::sin((h-6)*pi/12);
    return -10.f*std::sin(std::fmod(h+6,24.f)*pi/12);
}
inline float weatherDistraction(const Situation& f) {return f.outdoors?std::clamp(finite(f.weather),0.f,12.f):0.f;}
inline float reactionRetention(const Situation& f) {return f.bumpDistracted?1.f-std::clamp(finite(f.bumpEffect,1),.25f,1.f):1.f;}
inline float immediateReaction(const Situation& f) {
    if(f.sleeping)return 0;
    return reactionRetention(f)*((f.facing?22.f:0.f)+(f.close?10.f:0.f)+
        std::max(f.detected?18.f:0.f,std::clamp(finite(f.detection),0.f,1.f)*25));
}
// Saved separately so older attention-state records retain their exact layout.
struct AttentionTrail {double last=0;float reaction=0;std::uint32_t bumpType=0;float bumpEffect=1,wariness=0;double warinessLast=0;};
inline float sampleReaction(AttentionTrail& trail,const Situation& f,double now) {
    if(!std::isfinite(now))return 0;
    const float elapsed=now>=trail.last?float(now-trail.last):0.f;
    if(now<trail.last||f.sleeping)trail.reaction=0;
    trail.reaction=std::max(immediateReaction(f),std::max(0.f,finite(trail.reaction)-elapsed*5.f));
    trail.last=now;
    return f.sleeping?0.f:trail.reaction*reactionRetention(f);
}
inline float attention(const Situation& f) {
    if(f.ignorePersonalAwareness)return 0;
    if(f.sleeping)return 0;
    // A genuine bump window suppresses immediate contact/turn reactions,
    // while prior suspicion, failures, interactions and witnesses remain.
    const float reaction=std::max(immediateReaction(f),std::max(0.f,finite(f.lingeringReaction)));
    return std::clamp(finite(f.attention)+timeAwareness(f.hour,f.guard)+reaction+std::min(f.alerted,3u)*6-
        (f.sitting?8:0)-(f.speakingToPlayer?8:0)-(f.talking&&!f.facing?14:0)-(f.focusedElsewhere?12:0)-(f.fleeing&&!f.facing?16:0),0.f,100.f);
}
inline float distraction(const Situation& f) {
    return std::clamp((finite(f.misdirection)+weatherDistraction(f)+std::min(f.crowd,6u)*1.5f)/30.f,0.f,1.f);
}
inline float weightBurden(float weight) {return std::clamp(std::log1p(std::max(0.f,finite(weight)))/std::log(31.f),0.f,1.f);}
inline float valueBurden(unsigned value) {return std::clamp(std::log1p(float(value))/std::log(2001.f),0.f,1.f);}
inline float awarenessLoss(const Situation& f,const Config& c) {
    const float eyes=std::clamp((attention(f)+std::max(0.f,finite(f.witnessPenalty))*c.witness)/100.f,0.f,1.f);
    return c.pointSkew*c.awareness*eyes;
}
inline bool elderRecord(std::uint32_t race,std::uint32_t baseRace,std::uint32_t voice) {
    const auto elderRace=[](std::uint32_t id){return id==0x67CD8||id==0xA82BA;};
    return elderRace(race)||elderRace(baseRace)||voice==0x13AE2||voice==0x13AE1||voice==0x13AD7||voice==0x13AD6;
}
inline float ageBenefit(const Situation& f,const Config& c) {return c.profile*std::clamp(finite(f.ageRelief),0.f,1.f);}
inline float witnessLoss(const Situation& f,const Config& c) {
    return c.pointSkew*c.awareness*std::clamp(std::max(0.f,finite(f.witnessPenalty))*c.witness/100.f,0.f,1.f);
}
inline float skillCapRelief(const Situation& f,const Config& c,float skill) {
    const float trained=std::clamp(finite(skill),0.f,100.f)/100;
    const float lightness=1.f-weightBurden(f.weight);
    return trained*lightness*std::min(20.f,12.f+.8f*c.value*valueBurden(f.value)+.5f*c.weight*weightBurden(f.weight));
}
inline float skillStartingCeiling(const Config& c,float skill) {
    const float trained=std::clamp(finite(skill),0.f,100.f)/100.f;
    return c.maximum*(1.f-.4f*std::pow(1.f-trained,1.5f));
}
inline float stackChanceRetention(unsigned stacks,float skill) {
    const float trained=std::clamp(finite(skill),0.f,100.f)/100.f;
    const float perStack=.15f+.8f*std::pow(trained,1.5f);
    return std::pow(perStack,float(std::min(stacks,100u)));
}
struct TheftPressure {std::uint32_t stacks=0;double until=0;};
inline unsigned activeStacks(TheftPressure& pressure,double now) {
    if(!std::isfinite(now))return 0;
    if(now>=pressure.until)pressure={};
    return pressure.stacks;
}
inline void successfulStack(TheftPressure& pressure,double now,double seconds) {
    if(!std::isfinite(now))return;
    activeStacks(pressure,now);pressure.stacks=std::min(pressure.stacks,99u)+1;
    pressure.until=now+std::clamp(std::isfinite(seconds)?seconds:90.0,60.0,120.0);
}
// Transfer evidence is independent of instance ownership tracking: a whole
// transferred stack counts once, even if its container event is split/repeated.
struct TransferEvidence {
    std::uint32_t victim=0,item=0;double until=0;bool success=false,transferred=false,counted=false;
    unsigned amount=1,unitValue=0;float unitWeight=0;bool equipped=false,playerOwned=false;
    bool observe(std::uint32_t oldContainer,std::uint32_t newContainer,std::uint32_t form,int count,double now) {
        if(victim&&now<=until&&count>0&&oldContainer==victim&&newContainer==0x14&&form==item){transferred=true;return true;}
        return false;
    }
    bool consume() {if(!success||!transferred||counted)return false;counted=true;return true;}
};
inline float cap(const Situation& f,const Config& c,float skill=0) {
    const float d=distraction(f);
    // A skill/perk bonus cannot rescue an actively watched, undistracted theft.
    if(!f.ignorePersonalAwareness&&f.facing&&f.detected&&!f.sleeping&&!f.focusedElsewhere&&d<.7f)return 0;
    const float maximum=std::min(100.f,c.maximum+(f.bumpDistracted&&f.bumpBonus&&!f.sleeping?1.f:0.f));
    return stackChanceRetention(f.successfulStacks,skill)*(f.failed?.2f:1.f)*std::clamp(skillStartingCeiling(c,skill)+(f.bumpDistracted&&f.bumpBonus&&!f.sleeping?1.f:0.f)-awarenessLoss(f,c)+c.distraction*d+skillCapRelief(f,c,skill)-c.pointSkew*c.weight*weightBurden(f.weight)-
        c.pointSkew*c.value*valueBurden(f.value)-c.profile*(c.pointSkew*std::max(0.f,finite(f.profileRisk)-std::clamp(finite(f.ageRelief),0.f,1.f))+std::min(0.f,finite(f.profileRisk)-std::clamp(finite(f.ageRelief),0.f,1.f))),0.f,maximum);
}
inline float witnessCapEffect(const Situation& f,const Config& c,float skill) {
    auto withoutWitnesses=f;withoutWitnesses.witnessPenalty=0;
    return std::max(0.f,cap(withoutWitnesses,c,skill)-cap(f,c,skill));
}
inline float combinedAwarenessCapEffect(const Situation& f,const Config& c,float skill) {
    auto neutral=f;neutral.ignorePersonalAwareness=true;neutral.witnessPenalty=0;
    return std::max(0.f,cap(neutral,c,skill)-cap(f,c,skill));
}
inline std::string witnessDisplay(std::string_view original,int chance,float effect) {
    std::string result(original);const auto token=std::to_string(chance)+"%";
    const auto position=result.find(token);
    if(position==std::string::npos||(position>0&&result[position-1]>='0'&&result[position-1]<='9'))return result;
    const auto rounded=std::lround(std::clamp(finite(effect),0.f,100.f));
    std::string extra=" ("+std::to_string(rounded)+"%)";
    const auto after=position+token.size();
    if(after==result.size()||(result[after]!=' '&&result[after]!='\t'&&result[after]!='\n'))extra+=' ';
    result.insert(after,extra);return result;
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
    const float raw=stackChanceRetention(f.successfulStacks,skill)*(f.failed?.2f:1.f)*(finite(hand)+(f.bumpDistracted&&f.bumpBonus&&!f.sleeping?1.f:0.f)+sleepRelief(f,c)+ageBenefit(f,c)-witnessLoss(f,c)+c.distraction*distraction(f)*(.5f+trained));
    return int(std::floor(std::clamp(finite(raw),0.f,cap(f,c,skill))));
}
inline float personalAwarenessEffect(float hand,float skill,const Situation& f,const Config& c) {
    if(f.sleeping)return 0;
    auto neutral=f;neutral.ignorePersonalAwareness=true;
    return float(std::max(0,finalChance(hand,skill,neutral,c)-finalChance(hand,skill,f,c)));
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
inline float bumpDistraction(unsigned type) {return type==1?36.f:24.f;}
inline double bumpDuration(unsigned type) {return type==1?10.0:6.0;}
inline float bumpEffectiveness(AttentionTrail& trail,double now) {
    if(!std::isfinite(now))return 1;
    if(now<trail.warinessLast)trail.wariness=0;
    else trail.wariness=std::max(0.f,finite(trail.wariness)-float(now-trail.warinessLast)/30.f);
    trail.wariness=std::clamp(trail.wariness,0.f,3.f);trail.warinessLast=now;
    return 1.f-.25f*trail.wariness;
}
inline void witnessBump(AttentionTrail& trail,double now) {
    if(!std::isfinite(now))return;
    bumpEffectiveness(trail,now);trail.wariness=std::min(3.f,trail.wariness+1);
}
inline void bump(AttentionState& s,double now,unsigned type=0,float effect=1) {
    if(type>1)return;
    effect=std::clamp(finite(effect,1),.25f,1.f);
    decay(s,now);
    // A native small-to-big reaction may be one collision. Upgrade its effect
    // without counting a second bump or extending a repeated callback forever.
    if(now-s.lastBump<1) {
        if(type==1&&s.bumpUntil>now)s.bumpUntil=std::max(s.bumpUntil,s.lastBump+bumpDuration(type)*effect);
        return;
    }
    s.lastBump=now;
    if (++s.bumps<=2) s.bumpUntil=now+bumpDuration(type)*effect;
    else {s.bumpUntil=0;s.suspicion=std::min(60.f,s.suspicion+15);}
}
inline void failedTheft(AttentionState& s,double now) {
    decay(s,now);s.failedUntil=now+60;s.bumpUntil=0;s.suspicion=std::max(s.suspicion,40.f);
}
inline void interaction(AttentionState& s,double now,bool enabled=true) {
    decay(s,now);
    if(!enabled){s.interactionUntil=0;return;}
    if(s.interactionUntil>now)s.suspicion=std::min(60.f,s.suspicion+5);
    s.interactionUntil=now+15;
}
// Transient observations are deliberately not saved: loading/re-entering range
// establishes a baseline rather than replaying an old engine bump.
struct BumpObservation {
    float stamp=0;int type=-1;bool initialized=false;double contactUntil=-1;bool recognizedPlayer=false;
};
inline bool nativeBump(BumpObservation& observation,float stamp,int type,bool playerContact,double now) {
    if(!std::isfinite(stamp)||!std::isfinite(now)){observation={};return false;}
    if(playerContact)observation.contactUntil=now+.6;
    const bool changed=observation.initialized&&(stamp!=observation.stamp||type!=observation.type);
    observation.stamp=stamp;observation.type=type;
    if(!observation.initialized){observation.initialized=true;return false;}
    return changed&&(type==0||type==1)&&now<=observation.contactUntil;
}
inline bool witnessCounts(bool follower,bool guild,bool outlaw,bool friendOfVictim,bool sameFaction) {
    if(follower) return false;
    if(guild) return friendOfVictim;
    if(outlaw) return sameFaction||friendOfVictim;
    return true;
}
inline float relativeViewAngle(float angle,float focusAngle) {
    if(!std::isfinite(angle)||!std::isfinite(focusAngle))return 180.f;
    return std::remainder(angle-focusAngle,360.f);
}
inline bool witnessArea(bool sameCell,bool bothExterior,bool sameWorldspace) {return sameCell||(bothExterior&&sameWorldspace);}
inline bool witnessVisible(bool awake,float distance,float playerAngle,float victimAngle,bool playerLOS,bool victimLOS) {
 return awake&&std::isfinite(distance)&&distance<900&&std::isfinite(playerAngle)&&std::isfinite(victimAngle)&&std::abs(playerAngle)<110&&std::abs(victimAngle)<110&&playerLOS&&victimLOS;
}
inline double memoryDays(unsigned value,float scale) {return double(scale)*(1+std::log1p(double(value))/4);}
struct Stolen {
    std::uint32_t victim=0,form=0,owner=0,container=0,uidBase=0,value=0;
    std::uint16_t uid=0;bool reported=false;
    double noticed=0,expires=0,recognizesUntil=0;
    double lastTagUpdate=-1;
    std::uint32_t originHold=0;double stolenDay=-1;bool jurisdictionHidden=false;
};
// Absolute expiry already accounts for normal aging. Storage adds one extra
// day of progress per elapsed day; location changes settle the previous rate.
inline void ageStoredTag(Stolen& item,double now,bool stored,bool enabled) {
    if(!std::isfinite(now))return;
    if(item.lastTagUpdate<0){item.lastTagUpdate=now;return;}
    if(now<item.lastTagUpdate)return;
    if(stored&&enabled){
        const auto remaining=std::max(0.0,item.expires-item.lastTagUpdate);
        item.expires-=std::min(now-item.lastTagUpdate,remaining/2.0);
    }
    item.lastTagUpdate=now;
}

}

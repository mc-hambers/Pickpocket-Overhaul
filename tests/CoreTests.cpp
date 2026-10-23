#include "BumpRules.h"
#include "EquippedDrops.h"
#include "Karma.h"
#include "Core.h"
#include "GuardRules.h"
#include "Contraband.h"
#include "Commotion.h"
#include "Compatibility.h"
#include "ChanceSites.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <source_location>
#include <fstream>
#include <iterator>
int main(int argc,char** argv){
if(argc==4){std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});auto calls=PO::Hooks::discover(bytes,std::stoul(argv[2],nullptr,16));if(calls.size()!=1)return 2;auto site=PO::Hooks::equippedTransferSite(bytes,calls[0]);std::cout<<"Verified equipped transfer call RVA 0x"<<std::hex<<site.rva<<"\n";return 0;}
if(argc==3){std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});auto site=PO::Hooks::karmaSite(bytes,static_cast<std::uint32_t>(std::stoul(argv[2],nullptr,16)));std::cout<<"Verified karma RNG call RVA 0x"<<std::hex<<site.rva<<"\n";return 0;}
using namespace PO;int checks=0;auto check=[&](bool b,const std::source_location location=std::source_location::current()){++checks;if(!b){std::cerr<<"Failed behavior check at line "<<location.line()<<" (check "<<checks<<")\n";throw std::runtime_error("situational chance regression");}};// Synthetic executable exercises the same discovery/validation code as the DLL.
std::vector<std::uint8_t> image(1024,0x90);
auto put16=[&](size_t p,std::uint16_t v){std::memcpy(image.data()+p,&v,2);};
auto put32=[&](size_t p,std::uint32_t v){std::memcpy(image.data()+p,&v,4);};
put16(0,0x5A4D);put32(0x3C,0x80);put32(0x80,0x4550);put16(0x84,0x8664);put16(0x86,1);put16(0x94,0xF0);put16(0x98,0x20B);
const size_t section=0x188;put32(section+12,0x1000);put32(section+16,512);put32(section+20,512);put32(section+36,0x20000000);
for(size_t p:{size_t(544),size_t(608)}){image[p]=0xE8;put32(p+1,std::uint32_t(0x2000-(0x1000+p-512+5)));}
auto sites=Hooks::discover(image,0x2000);check(sites.size()==2);check(sites[0].rva==0x1020);check(sites[1].rva==0x1060);
std::array<std::uint8_t,21> live;std::memcpy(live.data(),image.data()+sites[0].fileOffset-8,21);
check(Hooks::contextMatches(image,sites[0],live));
std::int32_t redirected=-0x123456;std::memcpy(live.data()+9,&redirected,4);
check(Hooks::contextMatches(image,sites[0],live));check(Hooks::destination(live,8,0x100000)==0x100005+redirected);
live[8]=0xE9;check(!Hooks::contextMatches(image,sites[0],live));live[8]=0xE8;live[13]^=1;check(!Hooks::contextMatches(image,sites[0],live));
image[544]=0x90;check(Hooks::discover(image,0x2000).size()==1);
bool rejected=false;try{Hooks::discover(std::span<const std::uint8_t>(image.data(),100),0x2000);}catch(const std::exception&){rejected=true;}check(rejected);
auto rngImage=image;
const std::array<std::uint8_t,21> rngPattern{0xBA,0x64,0,0,0,0x48,0x8D,0x0D,0,0,0,0,0xE8,0,0,0,0,0x3B,0xC3,0x7D,0x12};
std::copy(rngPattern.begin(),rngPattern.end(),rngImage.begin()+700);
auto rngSite=Hooks::karmaSite(rngImage,0x1000);check(rngSite.fileOffset==712&&rngSite.rva==0x10C8);
auto ambiguous=rngImage;std::copy(rngPattern.begin(),rngPattern.end(),ambiguous.begin()+800);
rejected=false;try{Hooks::karmaSite(ambiguous,0x1000);}catch(const std::exception&){rejected=true;}check(rejected);
rngImage[717]=0x90;rejected=false;try{Hooks::karmaSite(rngImage,0x1000);}catch(const std::exception&){rejected=true;}check(rejected);
Config defaults;check(defaults.maximum==97&&defaults.distance==.45f&&defaults.pointSkew==1);Config c;c.maximum=98;Situation quiet;quiet.attention=0;
check(std::abs(cap(quiet,c)-58.8f)<.001f);Situation watched=quiet;watched.facing=true;watched.detected=true;check(cap(watched,c)==0);
for(int skill=0;skill<=150;++skill)for(unsigned mass=0;mass<70;++mass){Situation f;f.weight=float(mass);f.value=mass*100;f.attention=float(mass);f.misdirection=float(mass%30);f.witnessPenalty=float(mass%20);auto chance=finalChance(handChance(f.weight,f.value,c,float(skill)),float(skill),f,c);check(chance>=0&&chance<=cap(f,c,float(skill)));check(finalChance(1000,float(skill),watched,c)==0);}
Situation heavy=quiet;heavy.weight=30;check(std::abs(cap(heavy,c)-43.8f)<.001f);Situation expensive=quiet;expensive.value=2000;check(std::abs(cap(expensive,c)-48.8f)<.001f);Situation guard=quiet;guard.profileRisk=1;check(std::abs(cap(guard,c)-51.3f)<.001f);Situation elder=guard;elder.profileRisk=-.75f;check(cap(elder,c)>=cap(guard,c));
Situation eyes=quiet;eyes.attention=80;Situation chaos=eyes;chaos.misdirection=30;check(cap(chaos,c)>cap(eyes,c));Situation witnesses=eyes;witnesses.witnessPenalty=20;check(cap(witnesses,c)<cap(eyes,c));
check(handChance(10,1000,c,100)==handChance(10,1000,c,15));check(finalChance(1000,100,heavy,c)<=83);
AttentionState s;bump(s,10);bump(s,12);bump(s,14);check(s.suspicion>0&&s.bumpUntil==0);decay(s,500);check(s.suspicion==0);
check(!witnessCounts(true,false,false,true,true));check(!witnessCounts(false,true,false,false,true));check(witnessCounts(false,true,false,true,false));check(!witnessCounts(false,false,true,false,false));check(witnessCounts(false,false,true,false,true));check(memoryDays(1000,1)>memoryDays(10,1));
check(witnessVisible(true,300,0,20,true,true));
check(witnessVisible(true,300,100,100,true,true));
check(!witnessVisible(true,300,110,0,true,true));
check(witnessVisible(true,300,relativeViewAngle(170,170),relativeViewAngle(-175,170),true,true));
check(std::abs(relativeViewAngle(-175,170)-15)<.001f);
check(witnessArea(true,false,false));check(witnessArea(false,true,true));
check(!witnessArea(false,true,false));check(!witnessArea(false,false,true));
check(Config{}.recognitionDays==1);
check(!witnessVisible(true,300,150,20,true,true));
check(!witnessVisible(true,300,0,150,true,true));
check(!witnessVisible(true,300,0,20,false,true));
check(!witnessVisible(true,300,0,20,true,false));
check(!witnessVisible(false,300,0,20,true,true));
check(!witnessVisible(true,900,0,20,true,true));
BumpObservation observed;
check(!nativeBump(observed,0,-1,false,0)); // establish baseline
check(nativeBump(observed,1,0,true,1));
check(!nativeBump(observed,1,0,true,1.1)); // same native event is not replayed
check(!nativeBump(observed,2,0,false,2)); // no recent player contact
check(nativeBump(observed,3,1,true,3)); // large native bump qualifies too
check(nativeBump(observed,4,0,false,3.4)); // short attribution grace after contact
check(!nativeBump(observed,5,0,false,4));
BumpObservation stale;check(!nativeBump(stale,5,0,true,10)); // no load replay
AttentionState bumped;bump(bumped,1);check(bumped.bumpUntil==7);bump(bumped,1.1);check(bumped.bumps==1);bump(bumped,3);bump(bumped,5);check(bumped.bumpUntil==0&&bumped.suspicion>0);
Situation speaking;auto beforeSpeech=attention(speaking);speaking.speakingToPlayer=true;check(attention(speaking)==beforeSpeech-8);
speaking.facing=true;speaking.detected=true;check(cap(speaking,c)==0); // greeting is not blindness
Situation sleeper;sleeper.sleeping=true;sleeper.facing=true;sleeper.detected=true;sleeper.close=true;sleeper.detection=1;sleeper.attention=100;sleeper.alerted=3;sleeper.value=20;
check(attention(sleeper)==0);check(cap(sleeper,c)>0);
for(int skill:{0,15,50,100})for(float weight:{0.f,1.f,5.f,30.f})for(unsigned value:{0u,20u,1000u}){
 auto sleeping=sleeper;sleeping.weight=weight;sleeping.value=value;
 Situation unaware;sleeping.profileRisk=unaware.profileRisk=.4f;unaware.attention=0;unaware.weight=weight;unaware.value=value;
 check(cap(sleeping,c)==cap(unaware,c));
 const auto hand=handChance(weight,value,c,float(skill));
 check(finalChance(hand,float(skill),sleeping,c)>=finalChance(hand,float(skill),unaware,c));
 check(finalChance(hand,float(skill),sleeping,c)<=cap(sleeping,c,float(skill)));
 auto witnessed=sleeping;witnessed.witnessPenalty=60;check(cap(witnessed,c)<cap(sleeping,c));
 check(sleepRelief(witnessed,c)<=sleepRelief(sleeping,c));
 check(finalChance(100,float(skill),witnessed,c)<finalChance(100,float(skill),sleeping,c));
}
Situation relief;relief.attention=60;check(std::abs(sleepRelief(relief,c)-0.f)<.001f);check(finalChance(40,100,relief,c)==40); // isolate sleep relief from the skill ceiling
relief.sleeping=true;check(std::abs(sleepRelief(relief,c)-30.f)<.001f);check(finalChance(40,100,relief,c)==70);
relief.attention=20;check(std::abs(sleepRelief(relief,c)-10.f)<.001f);check(finalChance(40,100,relief,c)==50);
relief.attention=0;check(std::abs(sleepRelief(relief,c)-0.f)<.001f);check(finalChance(40,100,relief,c)==40);
relief.attention=60;relief.witnessPenalty=100;check(std::abs(sleepRelief(relief,c)-0.f)<.001f);check(finalChance(100,15,relief,c)==int(cap(relief,c,15)));
relief.witnessPenalty=40;check(std::abs(sleepRelief(relief,c)-30.f)<.001f);
Config noAwareness=c;noAwareness.awareness=0;check(std::abs(sleepRelief(relief,noAwareness)-0.f)<.001f);
Config halfAwareness=c;halfAwareness.awareness=25;check(std::abs(sleepRelief(relief,halfAwareness)-15.f)<.001f);
auto awake=sleeper;awake.sleeping=false;check(cap(awake,c)==0);check(std::abs(sleepRelief(sleeper,c)-50.f)<.001f); // hard-watch block is not converted into a 100-point bonus
Situation distracted;distracted.facing=true;distracted.detected=true;distracted.misdirection=24;check(cap(distracted,c)>0);
check(elderRecord(0x67CD8,0,0));check(elderRecord(0,0xA82BA,0));
for(unsigned voice:{0x13AE2u,0x13AE1u,0x13AD7u,0x13AD6u})check(elderRecord(0,0,voice));check(!elderRecord(0x13746,0x13746,0));
Situation witnessedTarget;witnessedTarget.attention=0;witnessedTarget.witnessPenalty=12;
Config noWitness=c;noWitness.witness=0;Config strongWitness=c;strongWitness.witness=3;
check(finalChance(40,15,witnessedTarget,noWitness)>finalChance(40,15,witnessedTarget,c));check(finalChance(40,15,witnessedTarget,c)>finalChance(40,15,witnessedTarget,strongWitness));
Situation old=witnessedTarget;old.witnessPenalty=0;old.ageRelief=.75f;auto young=old;young.ageRelief=0;
check(finalChance(40,15,old,c)>finalChance(40,15,young,c));check(std::abs(ageBenefit(old,c)-5.625f)<.001f);
Config ageDisabled=c;ageDisabled.profile=0;check(finalChance(40,15,old,ageDisabled)==finalChance(40,15,young,ageDisabled));
Situation beforeContact;beforeContact.attention=25;
Situation bumpReaction=beforeContact;bumpReaction.bumpDistracted=true;bumpReaction.bumpBonus=true;bumpReaction.facing=true;bumpReaction.close=true;bumpReaction.detected=true;bumpReaction.detection=1;bumpReaction.misdirection=24;
check(attention(bumpReaction)==attention(beforeContact));
for(float skill:{15.f,50.f,100.f})for(float weight:{0.f,5.f,30.f}){
 beforeContact.weight=bumpReaction.weight=weight;const auto hand=handChance(weight,20,c,skill);
 check(finalChance(hand,skill,bumpReaction,c)>=finalChance(hand,skill,beforeContact,c));
}
auto suspiciousBump=bumpReaction;suspiciousBump.attention+=30;check(attention(suspiciousBump)>attention(bumpReaction));
auto witnessedBump=bumpReaction;witnessedBump.witnessPenalty=40;check(finalChance(40,15,witnessedBump,c)<finalChance(40,15,bumpReaction,c));
auto expiredBump=bumpReaction;expiredBump.bumpDistracted=false;expiredBump.misdirection=0;check(cap(expiredBump,c)==0);
AttentionState repeatContact;bump(repeatContact,1);bump(repeatContact,3);check(repeatContact.bumpUntil==9);bump(repeatContact,5);check(repeatContact.bumpUntil==0&&repeatContact.suspicion>0);
Situation purse;purse.value=20;purse.attention=25;
check(cap(purse,c,100)>cap(purse,c,15));check(cap(purse,c,100)>95);check(cap(purse,c,100)<=c.maximum);
check(handChance(0,20,c,0)==handChance(0,20,c,100));
check(finalChance(handChance(0,20,c,100)+20,100,purse,c)>=95);
auto backpack=purse;backpack.weight=30;check(skillCapRelief(backpack,c,100)==0);check(cap(backpack,c,100)<cap(purse,c,100));
for(float skill:{0.f,15.f,50.f,100.f})check(cap(watched,c,skill)==0);
Config limited=c;limited.maximum=80;check(cap(purse,limited,100)<=80);
check(std::abs(timeAwareness(12,false)-5)<.001);check(std::abs(timeAwareness(0,false)+10)<.001);
for(float hour:{0.f,6.f,12.f,18.f,23.f})check(timeAwareness(hour,true)==0);
Situation daylight;daylight.hour=12;auto night=daylight;night.hour=0;check(attention(night)<attention(daylight));
daylight.guard=night.guard=true;check(attention(night)==attention(daylight));
Situation rain;rain.outdoors=true;rain.weather=6;auto storm=rain;storm.weather=12;check(distraction(storm)>distraction(rain));
rain.outdoors=false;check(weatherDistraction(rain)==0);storm.guard=true;check(weatherDistraction(storm)==12);
for(float skill:{0.f,15.f,50.f,100.f})for(float mass:{0.f,1.f,5.f,30.f}){auto target=purse;target.weight=mass;check(cap(target,c,skill)<=c.maximum);check(finalChance(120,skill,target,c)<=cap(target,c,skill));}
AttentionState inspecting;
interaction(inspecting,1,false);interaction(inspecting,2,false);check(inspecting.suspicion==0&&inspecting.interactionUntil==0);
interaction(inspecting,3,true);check(inspecting.interactionUntil==18&&inspecting.suspicion==0);
interaction(inspecting,4,true);check(inspecting.suspicion==5);
interaction(inspecting,5);check(inspecting.interactionUntil==20&&inspecting.suspicion>5);
// Attention rises immediately, falls over time, and cannot decay from repeated
// percentage queries while the game clock is paused.
AttentionTrail trail;Situation front;front.facing=true;front.close=true;front.detected=true;front.detection=1;
check(sampleReaction(trail,front,10)==57);
Situation rear;rear.attention=25;
rear.lingeringReaction=sampleReaction(trail,rear,10.1);check(std::abs(rear.lingeringReaction-56.5f)<.001f);
const auto rememberedCap=cap(rear,c,100);check(rememberedCap<cap(Situation{},c,100));
check(sampleReaction(trail,rear,10.1)==rear.lingeringReaction);
rear.lingeringReaction=sampleReaction(trail,rear,15);check(rear.lingeringReaction==32);check(cap(rear,c,100)>rememberedCap);
check(sampleReaction(trail,rear,30)==0);
check(sampleReaction(trail,front,31)==57);
auto slopeBump=front;slopeBump.bumpDistracted=true;slopeBump.bumpBonus=true;slopeBump.misdirection=24;
slopeBump.lingeringReaction=sampleReaction(trail,slopeBump,31.1);check(slopeBump.lingeringReaction==0);check(attention(slopeBump)==25);
rear.lingeringReaction=sampleReaction(trail,rear,32);check(rear.lingeringReaction==52); // normal decay continues beneath the bypass
Situation sleepingTrail; sleepingTrail.sleeping=true;check(sampleReaction(trail,sleepingTrail,33)==0&&trail.reaction==0);
sampleReaction(trail,front,40);check(sampleReaction(trail,Situation{},5)==0); // rolled-back clock
// Only a real active bump unlocks the extra percentage point. It is a cap,
// not guaranteed success, and never exceeds 100.
Situation bumpCeiling; bumpCeiling.attention=0;bumpCeiling.bumpDistracted=true;bumpCeiling.bumpBonus=true;
for(float maximum:{50.f,98.f,99.f,100.f}){
 Config limit=c;limit.maximum=maximum;
 auto ordinary=bumpCeiling;ordinary.bumpDistracted=false;
 check(finalChance(1000,100,ordinary,limit)==int(maximum));
 check(finalChance(1000,100,bumpCeiling,limit)==int(std::min(100.f,maximum+1)));
 check(finalChance(20,100,bumpCeiling,limit)==21);
}
auto bumpWithWitness=bumpCeiling;bumpWithWitness.witnessPenalty=45;check(finalChance(40,15,bumpWithWitness,c)<finalChance(40,15,bumpCeiling,c));
AttentionState caught;bump(caught,1);failedTheft(caught,2);check(caught.failedUntil==62&&caught.bumpUntil==0&&caught.suspicion>=40);
failedTheft(caught,3);check(caught.failedUntil==63);
for(bool distractedAfterFailure:{false,true})for(float skill:{0.f,15.f,100.f})for(float weight:{0.f,5.f,30.f}){
 Situation retry;retry.weight=weight;retry.value=20;retry.bumpDistracted=distractedAfterFailure;retry.misdirection=distractedAfterFailure?24.f:0.f;
 const auto before=finalChance(120,skill,retry,c);const auto beforeCap=cap(retry,c,skill);
 retry.failed=true;check(finalChance(120,skill,retry,c)<=int(std::ceil(before*.2f)));check(std::abs(cap(retry,c,skill)-beforeCap*.2f)<.001f);check(cap(retry,c,skill)<=20);
}
BumpObservation bothSizes;
check(!nativeBump(bothSizes,0,-1,false,0));
check(nativeBump(bothSizes,1,0,true,1));
check(nativeBump(bothSizes,1,1,false,1.1)); // native reaction upgrade
check(!nativeBump(bothSizes,1,1,true,1.2)); // repeated same event
check(!nativeBump(bothSizes,2,2,true,2)); // unknown enum is not a bump
check(!nativeBump(bothSizes,3,-1,true,3));
check(!nativeBump(bothSizes,4,1,false,4)); // no contact
AttentionState largeBump;bump(largeBump,1,1);check(largeBump.bumpUntil==11&&largeBump.bumps==1);
bump(largeBump,1.2,1);check(largeBump.bumpUntil==11&&largeBump.bumps==1);
AttentionState upgrade;bump(upgrade,1,0);bump(upgrade,1.1,1);check(upgrade.bumpUntil==11&&upgrade.bumps==1);
bump(upgrade,3,0);check(upgrade.bumpUntil==9&&upgrade.bumps==2); // a later small bump uses its own duration
bump(upgrade,5,1);check(upgrade.bumpUntil==0&&upgrade.bumps==3&&upgrade.suspicion>0);
AttentionState invalidBump;bump(invalidBump,1,2);check(invalidBump.bumps==0&&invalidBump.bumpUntil==0);
check(bumpDistraction(1)>bumpDistraction(0));check(bumpDuration(1)>bumpDuration(0));
for(float skill:{0.f,15.f,100.f}){
 Situation lightBump;lightBump.bumpDistracted=true;lightBump.bumpBonus=true;lightBump.misdirection=bumpDistraction(0);
 auto strongBump=lightBump;strongBump.misdirection=bumpDistraction(1);
 check(distraction(strongBump)>distraction(lightBump));
 check(finalChance(40,skill,strongBump,c)>finalChance(40,skill,lightBump,c));
 check(finalChance(1000,skill,strongBump,c)<=99);
 strongBump.failed=true;check(finalChance(1000,skill,strongBump,c)<=19);
}
// Watching other victims builds personal wariness; the effect decays rather
// than being shared globally or reset by another bump on the same NPC.
AttentionTrail watcher,unawareWatcher;
check(bumpEffectiveness(watcher,10)==1);
witnessBump(watcher,10);check(bumpEffectiveness(watcher,10)==.75f);
witnessBump(watcher,10);check(bumpEffectiveness(watcher,10)==.5f);
witnessBump(watcher,10);check(bumpEffectiveness(watcher,10)==.25f);
witnessBump(watcher,10);check(watcher.wariness==3&&bumpEffectiveness(watcher,10)==.25f);
check(bumpEffectiveness(unawareWatcher,10)==1);
check(bumpEffectiveness(watcher,40)==.5f);
check(bumpEffectiveness(watcher,100)==1&&watcher.wariness==0);
witnessBump(watcher,110);check(bumpEffectiveness(watcher,5)==1); // rollback safety
AttentionState cautiousSmall,cautiousLarge;
bump(cautiousSmall,10,0,.75f);bump(cautiousLarge,10,1,.75f);
check(cautiousSmall.bumpUntil==14.5&&cautiousLarge.bumpUntil==17.5);
bump(cautiousSmall,10.1,1,.75f);check(cautiousSmall.bumps==1&&cautiousSmall.bumpUntil==17.5);
Situation successfulBump;successfulBump.bumpDistracted=true;successfulBump.bumpBonus=true;successfulBump.misdirection=bumpDistraction(1);successfulBump.detected=true;successfulBump.detection=1;successfulBump.close=true;
auto waryBump=successfulBump;waryBump.bumpEffect=.75f;waryBump.misdirection=bumpDistraction(1)*waryBump.bumpEffect;
check(attention(waryBump)>attention(successfulBump));
check(finalChance(40,15,waryBump,c)<finalChance(40,15,successfulBump,c));
Config visibilityBalance=c;visibilityBalance.awareness=100;visibilityBalance.distraction=10;check(cap(waryBump,visibilityBalance,100)<cap(successfulBump,visibilityBalance,100));
AttentionTrail waryTrail;sampleReaction(waryTrail,front,10);
waryBump.lingeringReaction=sampleReaction(waryTrail,waryBump,10.1);check(waryBump.lingeringReaction>0);
check(attention(waryBump)>attention(successfulBump));
const auto frozenEffect=waryBump.bumpEffect;witnessBump(waryTrail,12);check(waryBump.bumpEffect==frozenEffect); // snapshot for the active collision
for(float effect:{1.f,.75f,.5f,.25f}){
 auto target=successfulBump;target.bumpEffect=effect;target.misdirection=bumpDistraction(1)*effect;
 check(finalChance(1000,100,target,c)<=99);
 target.failed=true;check(finalChance(1000,100,target,c)<=19);
}
// Skill now changes the starting situational ceiling, not post-perk execution.
check(std::abs(skillStartingCeiling(c,0)-58.8f)<.001f);
check(skillStartingCeiling(c,15)>67&&skillStartingCeiling(c,15)<68);
check(skillStartingCeiling(c,50)>84&&skillStartingCeiling(c,50)<85);
check(skillStartingCeiling(c,100)==98);
for(int skill=1;skill<=100;++skill)check(skillStartingCeiling(c,float(skill))>skillStartingCeiling(c,float(skill-1)));
Situation logged120;logged120.value=120;logged120.weight=.1f;logged120.attention=53.9f;logged120.witnessPenalty=4.7f;logged120.ageRelief=.75f;
const auto ceiling120=cap(logged120,c,17);const int replay120=finalChance(71.9f,17,logged120,c);
check(ceiling120>=39&&ceiling120<=43);check(replay120==int(ceiling120));
Situation logged160;logged160.value=160;logged160.weight=.25f;logged160.attention=62.8f;logged160.witnessPenalty=10;
const auto ceiling160=cap(logged160,c,16);const int replay160=finalChance(70.7f,16,logged160,c);
check(ceiling160>=24&&ceiling160<=29);check(replay160==int(ceiling160));
Situation precision;precision.attention=0;
check(finalChance(20,15,precision,c)==20); // no automatic handling subtraction
check(finalChance(40,15,precision,c)==40);
check(finalChance(20,100,precision,c)==20);
check(finalChance(60,15,precision,c)>finalChance(40,15,precision,c)); // perk input still controls execution below cap
for(float skill:{0.f,15.f,50.f,100.f}){
 auto bumped=logged120;bumped.bumpDistracted=true;bumped.bumpBonus=true;bumped.misdirection=36;
 check(cap(bumped,c,skill)>=cap(logged120,c,skill));
 check(cap(bumped,c,skill)<=99);
 check(finalChance(120,skill,bumped,c)<=cap(bumped,c,skill));
 check(finalChance(120,skill,watched,c)==0);
}
std::cout<<"Ceiling-based log replays: value 120 = "<<replay120<<"%, value 160 = "<<replay160<<"% (not live gameplay)\n";
Situation witnessMeter;witnessMeter.attention=25;witnessMeter.witnessPenalty=20;
check(std::abs(witnessCapEffect(witnessMeter,c,50)-10)<.001f);
check(witnessDisplay("Chance: 64%",64,10)=="Chance: 64% (10%) ");
check(witnessDisplay("<font color='#FFFFFF'>64%</font> TO STEAL",64,10)=="<font color='#FFFFFF'>64% (10%) </font> TO STEAL");
check(witnessDisplay("64%",64,0)=="64% (0%) ");
check(witnessDisplay("64%",64,6.2f)=="64% (6%) ");
check(witnessDisplay("64%",64,-10)=="64% (0%) ");
check(witnessDisplay("164%",64,10)=="164%");
check(witnessDisplay("No chance text",64,10)=="No chance text");
Config witnessOff=c;witnessOff.witness=0;check(witnessCapEffect(witnessMeter,witnessOff,50)==0);
auto blockedMeter=witnessMeter;blockedMeter.facing=blockedMeter.detected=true;check(witnessCapEffect(blockedMeter,c,50)==0);
auto failureMeter=witnessMeter;failureMeter.failed=true;check(std::abs(witnessCapEffect(failureMeter,c,50)-2)<.001f);
Situation capSaturated;capSaturated.bumpDistracted=true;capSaturated.bumpBonus=true;capSaturated.misdirection=36;capSaturated.witnessPenalty=6;
check(witnessCapEffect(capSaturated,c,100)==0); // a raw witness penalty can be masked by the cap
Situation occupationMeter;occupationMeter.value=120;occupationMeter.weight=5;
auto guardMeter=occupationMeter;guardMeter.profileRisk=1;
check(std::abs(cap(occupationMeter,c,50)-cap(guardMeter,c,50)-7.5f)<.001f);
Config occupationOff=c;occupationOff.profile=0;check(cap(occupationMeter,occupationOff,50)==cap(guardMeter,occupationOff,50));
for(float skill:{0.f,15.f,50.f,100.f})for(float eyes:{0.f,5.f,20.f,45.f}){
 auto meter=witnessMeter;meter.witnessPenalty=eyes;
 check(witnessCapEffect(meter,c,skill)>=0);
 check(witnessCapEffect(meter,c,skill)<=witnessLoss(meter,c)+.001f);
}
for(int fixedSetting:{0,1,3,4,5})check(!editableOption(fixedSetting));
for(int playerSetting:{2,6,7,8,9})check(editableOption(playerSetting));
check(!editableOption(-1)&&editableOption(10)&&!editableOption(11));
AttentionState openingOn;interaction(openingOn,1,true);check(openingOn.interactionUntil==16);interaction(openingOn,2,true);check(openingOn.suspicion==5);
AttentionState openingOff;interaction(openingOff,1,false);interaction(openingOff,2,false);check(openingOff.interactionUntil==0&&openingOff.suspicion==0);
interaction(openingOn,3,false);check(openingOn.interactionUntil==0&&openingOn.suspicion>0&&openingOn.suspicion<5); // existing suspicion decays, not erased
interaction(openingOn,4,true);check(openingOn.interactionUntil==19&&openingOn.suspicion<5); // toggle does not replay an old interaction
AttentionState hiddenOpen;interaction(hiddenOpen,1,true);check(hiddenOpen.interactionUntil==16&&hiddenOpen.suspicion==0);interaction(hiddenOpen,2,true);check(hiddenOpen.suspicion==5);
// Successful transfers count stacks, not individual units; penalties survive renewed bonuses.
check(stackChanceRetention(0,15)==1);
check(stackChanceRetention(1,15)>.19f&&stackChanceRetention(1,15)<.20f);
check(std::abs(stackChanceRetention(1,100)-.95f)<.0001f);
for(float skill:{0.f,15.f,50.f,100.f}){
 for(unsigned stacks=1;stacks<10;++stacks){
  check(stackChanceRetention(stacks,skill)<stackChanceRetention(stacks-1,skill));
  auto pressured=successfulBump;pressured.successfulStacks=stacks;
  check(std::abs(cap(pressured,c,skill)-cap(successfulBump,c,skill)*stackChanceRetention(stacks,skill))<.001f);
  check(finalChance(1000,skill,pressured,c)<=cap(pressured,c,skill));
 }
}
TheftPressure pressure;successfulStack(pressure,10,90);check(pressure.stacks==1&&pressure.until==100);
successfulStack(pressure,50,120);check(pressure.stacks==2&&pressure.until==170);
check(activeStacks(pressure,169)==2);check(activeStacks(pressure,170)==0);
successfulStack(pressure,180,0);check(pressure.until==240);successfulStack(pressure,181,1000);check(pressure.until==301);
TheftPressure otherPressure;check(activeStacks(otherPressure,190)==0&&activeStacks(pressure,190)==2);
TransferEvidence evidence{0x123,0xF,12};
check(!evidence.observe(0x123,0x14,0xF,0,10));
check(!evidence.observe(0x14,0x123,0xF,20,10));
check(!evidence.observe(0x124,0x14,0xF,20,10));
check(!evidence.observe(0x123,0x14,0xE,20,10));
check(!evidence.observe(0x123,0x14,0xF,20,13));
check(evidence.observe(0x123,0x14,0xF,200,10));check(!evidence.consume());
evidence.success=true;check(evidence.consume());check(!evidence.consume());
check(evidence.observe(0x123,0x14,0xF,10,11));check(!evidence.consume());
TransferEvidence rollOnly{0x123,0xF,12,true};check(!rollOnly.consume());
// One player deck across attempts; inspecting/toggling need not mutate it.
KarmaRolls deck;deck.seed=987654321;check(deck.valid());
for(unsigned chance=1;chance<100;++chance){
 KarmaRolls sample;sample.seed=chance;
 for(unsigned cycle=0;cycle<4;++cycle){unsigned successes=0;std::array<bool,100> seen{};
  for(unsigned n=0;n<100;++n){auto roll=sample.draw();check(roll<100&&!seen[roll]);seen[roll]=true;successes+=roll<chance;}
  check(successes==chance);check(sample.valid());
 }
}
for(unsigned n=0;n<37;++n)deck.draw();
std::array<std::uint8_t,112> encodedDeck{};
std::memcpy(encodedDeck.data(),&deck.seed,8);std::memcpy(encodedDeck.data()+8,&deck.cursor,4);std::memcpy(encodedDeck.data()+12,deck.bag.data(),100);
KarmaRolls restoredDeck;std::memcpy(&restoredDeck.seed,encodedDeck.data(),8);std::memcpy(&restoredDeck.cursor,encodedDeck.data()+8,4);std::memcpy(restoredDeck.bag.data(),encodedDeck.data()+12,100);check(restoredDeck.valid());
for(unsigned n=0;n<500;++n)check(deck.draw()==restoredDeck.draw());
check(deck.seed==restoredDeck.seed&&deck.cursor==restoredDeck.cursor&&deck.bag==restoredDeck.bag);
auto corruptDeck=deck;corruptDeck.cursor=101;check(!corruptDeck.valid());
corruptDeck=deck;corruptDeck.bag[1]=corruptDeck.bag[0];check(!corruptDeck.valid());
corruptDeck=deck;corruptDeck.bag[0]=100;check(!corruptDeck.valid());
BadLuckProtection protection;
check(protection.effective(50)==50);
for(unsigned n=0;n<4;++n)protection.outcome(50,false);
check(protection.credit==200&&protection.effective(50)==55);
protection.outcome(50,false);check(protection.effective(50)==60);
auto savedProtection=protection;check(savedProtection.credit==protection.credit);
for(unsigned base=0;base<=100;++base)for(unsigned credit:{0u,150u,200u,500u,1000u}){
 protection.credit=credit;auto effective=protection.effective(base);
 check(effective>=base&&effective<=100);
 if(base==0||base==100)check(effective==base);
 for(unsigned roll=0;roll<100;++roll){
  check((protection.roll(base,roll)<base)==(roll<effective));
  if(roll<base)check(protection.roll(base,roll)==roll);
 }
}
protection.credit=1000;protection.outcome(0,false);check(protection.credit==1000);
protection.outcome(50,false);check(protection.credit==1000&&protection.valid());
protection.outcome(100,true);check(protection.credit==0);
protection.credit=1001;check(!protection.valid());
BumpRules rules;
check(rules.allowed(10,74,74));check(!rules.allowed(10,75,0));check(!rules.allowed(10,0,75));
rules.contact(10,100,true,true,true,true);check(rules.bonusUntil==100);
rules.contact(10,110,true,false,true,true);check(rules.bonusUntil==0);
rules.contact(10.01,120,true,true,true,true);check(rules.bonusUntil==0);
rules.contact(10.05,130,true,true,true,true);check(rules.bonusUntil==130);
rules.failed(10.1);check(!rules.allowed(11.09,0,0)&&rules.allowed(11.1,0,0)&&rules.bonusUntil==0);
rules.witnessed(12);check(!rules.allowed(12.01,0,0)&&rules.allowed(12.05,0,0));
BumpRules recognized;recognized.contact(10,100,true,true,false,true);check(recognized.bonusUntil==0);
for(bool unlocked:{false,true}){Config compat=c;compat.maximum=perkMaximum(unlocked);Situation free;free.attention=0;free.misdirection=30;
 check(finalChance(1000,100,free,compat)==(unlocked?99:89));free.bumpDistracted=true;free.bumpBonus=false;check(finalChance(1000,100,free,compat)==(unlocked?99:89));free.bumpBonus=true;check(finalChance(1000,100,free,compat)==(unlocked?100:90));}
check(nearbyDrop(30,0));check(nearbyDrop(120,64));check(nearbyDrop(138,1.5));check(nearbyDrop(180,0));check(nearbyDrop(181,0));check(nearbyDrop(270,0));check(!nearbyDrop(271,0));check(!nearbyDrop(50,65));check(!nearbyDrop(50,-65));check(!nearbyDrop(NAN,0));
check(matchesEquippedTransfer(1,2,3,4,1,2,3,4));
check(!matchesEquippedTransfer(1,2,3,4,5,2,3,4));
check(!matchesEquippedTransfer(1,2,3,4,1,5,3,4));
check(!matchesEquippedTransfer(1,2,3,4,1,2,5,4));
check(!matchesEquippedTransfer(1,2,3,4,1,2,3,5));
check(!matchesEquippedTransfer(1,2,0,4,1,2,0,4));
// Exhaust the independent drop eligibility bits, including asleep/reverse/fail.
for(unsigned bits=0;bits<256;++bits)for(int count:{0,1,2,10}) {
 bool enabled=bits&1,ready=bits&2,success=bits&4,taking=bits&8,sleeping=bits&16,worn=bits&32,supported=bits&64;
 check(shouldDropEquipped(enabled,ready,success,taking,sleeping,worn,supported,count)==(enabled&&ready&&success&&taking&&!sleeping&&worn&&supported&&count==1));
}
check(canRecoverDrop(true,true,false,false,false,false,100,true));
check(canRecoverDrop(true,true,false,false,true,true,100,true));
check(!canRecoverDrop(true,true,false,false,true,false,100,true));
check(!canRecoverDrop(true,true,true,false,false,true,100,true));
check(!canRecoverDrop(true,true,false,true,false,true,100,true));
check(canRecoverDrop(true,true,false,false,false,true,270,true));check(!canRecoverDrop(true,true,false,false,false,true,271,true));
check(!canRecoverDrop(true,true,false,false,false,true,100,false));
check(!canRecoverDrop(false,true,false,false,false,true,100,true));
check(!canRecoverDrop(true,false,false,false,false,true,100,true));
check(!canRecoverDrop(true,true,false,false,false,true,NAN,true));
std::vector<std::uint8_t> transferImage(128,0x90);
auto putTransfer=[&](std::size_t offset){const std::uint8_t bytes[]={0x48,0x8D,0x54,0x24,0x78,0x48,0x8B,0xCB,0xFF,0x95,0xB0,2,0,0};std::copy(std::begin(bytes),std::end(bytes),transferImage.begin()+offset-8);};
putTransfer(32);check(Hooks::equippedTransferSite(transferImage,{0x1000,0}).rva==0x1020);
transferImage[34]=0xB8;bool rejectedTransfer=false;try{Hooks::equippedTransferSite(transferImage,{0x1000,0});}catch(...){rejectedTransfer=true;}check(rejectedTransfer);
transferImage[34]=0xB0;putTransfer(80);rejectedTransfer=false;try{Hooks::equippedTransferSite(transferImage,{0x1000,0});}catch(...){rejectedTransfer=true;}check(rejectedTransfer);
check(witnessDisplay("64%TO STEAL",64,22)=="64% (22%) TO STEAL");
check(witnessDisplay("64% TO STEAL",64,22)=="64% (22%) TO STEAL");
Situation personal;personal.attention=50;personal.witnessPenalty=20;
check(personalAwarenessEffect(1000,50,personal,c)>0);
auto noEyes=personal;noEyes.ignorePersonalAwareness=true;check(awarenessLoss(noEyes,c)==witnessLoss(noEyes,c));
auto asleep=personal;asleep.sleeping=true;check(personalAwarenessEffect(1000,50,asleep,c)==0);
check(personalAwarenessEffect(0,50,personal,c)==0);
Situation combined;combined.attention=40;combined.witnessPenalty=20;
auto neutralCombined=combined;neutralCombined.ignorePersonalAwareness=true;neutralCombined.witnessPenalty=0;
check(std::abs(combinedAwarenessCapEffect(combined,c,50)-(cap(neutralCombined,c,50)-cap(combined,c,50)))<.001f);
combined.sleeping=true;check(combinedAwarenessCapEffect(combined,c,50)==witnessCapEffect(combined,c,50));
// Transient distractions never add unbounded relief or extend after expiry.
Commotion noise;check(noise.value(0)==0);
noise.add(Noise::Explosion,20,0,6);check(noise.value(1)==20);
for(int i=0;i<100;++i)noise.add(Noise::Explosion,20,0,6);
check(noise.value(1)==20);
noise.add(Noise::Argument,14,0,3);noise.add(Noise::Panic,6,0,2);
check(noise.value(1)==24);check(noise.value(4)==20);check(noise.expired(6));
noise.add(Noise::Hit,NAN,0,5);check(noise.value(7)==0);
check(noiseAtDistance(0,1000,20,true)==20);check(noiseAtDistance(500,1000,20,true)==10);
check(noiseAtDistance(500,1000,20,false)==0);check(noiseAtDistance(1000,1000,20,true)==0);
check(recoveryWalkStalled(true,false,false,13));check(!recoveryWalkStalled(true,false,true,60));check(!recoveryWalkStalled(true,true,false,13));
Situation afraid;afraid.attention=30;auto afraidNeutral=attention(afraid);afraid.fleeing=true;check(attention(afraid)<afraidNeutral);
afraid.facing=true;auto afraidFront=attention(afraid);afraid.fleeing=false;check(attention(afraid)==afraidFront);
check(witnessCounts(false,false,true,true,false));
auto mapped=mappedForm(" MyGuild.esp | 0x800 ");check(mapped&&mapped->plugin=="MyGuild.esp"&&mapped->localID==0x800);
check(!mappedForm("MyGuild.esp|FE000800"));check(!mappedForm("MyGuild.esp|800junk"));check(!mappedForm("MyGuild.esp|"));check(!mappedForm("|800"));check(!mappedForm("name|800"));
check(!loyaltyCounts(true,false,false,false,false,true,true,true));
check(!loyaltyCounts(false,true,false,false,false,false,false,false));
check(loyaltyCounts(false,true,false,false,false,false,true,false));
check(!loyaltyCounts(false,false,true,false,false,false,true,false)); // generic bandit alliance is not a camp link
check(loyaltyCounts(false,false,true,false,false,true,false,false));
check(loyaltyCounts(false,false,true,false,false,false,false,true));
check(loyaltyCounts(false,false,false,true,true,false,false,false));
check(!loyaltyCounts(false,false,false,true,false,false,true,false));
check(loyaltyCounts(false,false,false,false,false,false,false,false));
// Storage aging: normal time is absolute; containers add equal bonus time.
Stolen aging;aging.expires=10;aging.recognizesUntil=30;aging.lastTagUpdate=0;
ageStoredTag(aging,2,true,true);check(aging.expires==8);check(aging.recognizesUntil==30);
ageStoredTag(aging,4,false,true);check(aging.expires==8); // removal resumes ordinary rate
ageStoredTag(aging,5,true,true);check(aging.expires==7);
ageStoredTag(aging,5,true,true);check(aging.expires==7); // no deposit/tick duplication
ageStoredTag(aging,4,true,true);check(aging.lastTagUpdate==5);check(aging.expires==7);
ageStoredTag(aging,6,true,false);check(aging.expires==7); // disabled bonus is not banked
ageStoredTag(aging,6.5,true,true);check(aging.expires==6.5);
Stolen overdue;overdue.expires=10;overdue.lastTagUpdate=0;
ageStoredTag(overdue,100,true,true);check(overdue.expires==5); // long unloaded interval
Stolen legacyTag;legacyTag.expires=10;ageStoredTag(legacyTag,4,true,true);check(legacyTag.expires==10);check(legacyTag.lastTagUpdate==4);
Stolen savedTag=legacyTag;ageStoredTag(savedTag,6,true,true);check(savedTag.expires==8);
static_assert(offsetof(Stolen,lastTagUpdate)==56); // version-one co-save compatibility
RecoveryProgress search;
check(!search.advance(10,true));check(!search.advance(11,true));check(search.spent==1);
check(!search.advance(500,false));check(!search.advance(900,true));check(search.spent==1);
check(!search.advance(899,true));check(search.spent==1); // clocks moving backward do not spend time
for(int second=901;second<950;++second)search.advance(second,true);
check(search.spent==45);check(!search.advance(950,true));
RecoveryProgress resumeSearch;resumeSearch.spent=44;check(!resumeSearch.advance(100,true));check(!resumeSearch.advance(101,true));
RecoveryProgress pausedSearch;pausedSearch.advance(0,true);pausedSearch.advance(1000,true);check(pausedSearch.spent==2); // clamp a stalled main thread
TransferEvidence stackEvidence{1,0xF,10};stackEvidence.amount=100;stackEvidence.unitValue=1;check(stackEvidence.observe(1,0x14,0xF,100,1));stackEvidence.success=true;check(stackEvidence.consume());check(!stackEvidence.consume());check(stackEvidence.amount*stackEvidence.unitValue==100);
// Recognition never crosses jurisdictions for cheap/local valuables, but contraband and exceptional value do.
for(unsigned value:{0u,1u,99u,100u,999u,1000u,5000u})for(bool local:{false,true})for(bool recent:{false,true}){
 check(guardRecognizes(false,value,local,recent)==(value>=1000||(local&&(recent||value>=100))));
 check(guardRecognizes(true,value,local,recent));
}
Situation burden;burden.attention=50;burden.weight=2;burden.value=100;burden.witnessPenalty=15;burden.profileRisk=1;
Config easier=c,harder=c;easier.pointSkew=.5f;harder.pointSkew=2;
for(int skill=0;skill<=100;++skill){check(cap(burden,easier,float(skill))>=cap(burden,c,float(skill)));check(cap(burden,c,float(skill))>=cap(burden,harder,float(skill)));}
static_assert(offsetof(Stolen,originHold)==64); // version-two co-save compatibility
check(contrabandRecords.size()==12);
check(contrabandEnabled(true,false));check(!contrabandEnabled(true,true));check(!contrabandEnabled(false,false));
check(contrabandProviderName("Contraband Confiscation.esp"));check(contrabandProviderName("ContrabandConfiscation.esp"));check(contrabandProviderName("CONTRABAND_CONFISCATION.ESL"));
check(!contrabandProviderName("Contraband Confiscation - USSEP Patch.esp"));check(!contrabandProviderName("Unrelated.esp"));
std::cout<<"Passed "<<checks<<" behavior checks\n";}


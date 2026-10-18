// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. NO WARRANTY; see LICENSE.txt.
#include "Core.h"
#include "ChanceSites.h"
#include "StandingPickpocketAPI.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <spdlog/sinks/basic_file_sink.h>
namespace {
using namespace PO;using Clock=std::chrono::steady_clock;
Config config;std::recursive_mutex mutex;std::map<RE::FormID,AttentionState> states;
std::map<RE::FormID,AttentionTrail> attentionTrails;
std::vector<Stolen> stolen;std::atomic_bool active=false,queued=false;std::atomic_uint64_t session=0;
double playSeconds=0;Clock::time_point lastTick=Clock::now();std::jthread worker;
const StandingPickpocketAPI::V1* standing=nullptr;int ownerToken;
RE::TESFaction* follower=nullptr;std::vector<RE::TESFaction*> guilds,outlaws;
std::set<RE::FormID> animated;std::map<RE::FormID,BumpObservation> bumpObservations;
RE::NiPoint3 previousPlayerPosition{};bool havePlayerPosition=false;double nextFullTick=0;std::atomic_bool hookReady=false;std::atomic<float> cachedCap=-1;std::atomic_bool distanceOwned=false;
constexpr auto configPath=L"Data/SKSE/Plugins/PickpocketOverhaul.ini";
using ChanceFn=std::int32_t(*)(float,float,std::uint32_t,float,RE::Actor*,RE::Actor*,bool,RE::TESForm*);
ChanceFn original=nullptr;
ChanceFn previousChance[2]{};
using PickpocketAlarmFn=void(*)(RE::Actor*,RE::TESObjectREFR*,RE::TESForm*,std::int32_t);
PickpocketAlarmFn recognitionAlarm=nullptr;
float* option(int i) {
    switch(i) {case 0:return &config.awareness;case 1:return &config.distraction;case 2:return &config.distance;case 3:return &config.weight;case 4:return &config.value;case 5:return &config.profile;case 6:return &config.witness;case 7:return &config.tagDays;case 8:return &config.recognitionDays;case 9:return &config.maximum;default:return nullptr;}
}
constexpr const wchar_t* keys[]={L"AwarenessPoints",L"DistractionPoints",L"DistanceMultiplier",L"WeightPoints",L"ValuePoints",L"ProfilePoints",L"WitnessStrength",L"StolenTagDays",L"RecognitionDays",L"AbsoluteMaximum"};
void distanceOverride() {distanceOwned=standing&&standing->SetDistanceOverride(&ownerToken,config.distance);}
float getOption(RE::StaticFunctionTag*,int i) {std::scoped_lock lock(mutex);auto* p=option(i);return p?*p:0;}
void setOption(RE::StaticFunctionTag*,int i,float value) {
    {std::scoped_lock lock(mutex);auto* p=option(i);if(!p||!std::isfinite(value))return;*p=value;normalize(config);cachedCap=-1;
    for(int n=0;n<10;++n)WritePrivateProfileStringW(L"Settings",keys[n],std::to_wstring(*option(n)).c_str(),configPath);}
    if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([]{std::scoped_lock lock(mutex);distanceOverride();});
}
float currentCap(RE::StaticFunctionTag*) {return cachedCap.load();}
bool ready(RE::StaticFunctionTag*) {return hookReady.load();}
bool standingOwned(RE::StaticFunctionTag*) {return distanceOwned.load();}
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm) {vm->RegisterFunction("GetOption","PO_Native",getOption);vm->RegisterFunction("SetOption","PO_Native",setOption);vm->RegisterFunction("GetCurrentCap","PO_Native",currentCap);vm->RegisterFunction("IsReady","PO_Native",ready);vm->RegisterFunction("IsStandingDistanceOwned","PO_Native",standingOwned);return true;}
double days() {auto* c=RE::Calendar::GetSingleton();return c?c->GetDaysPassed():0;}
bool following(RE::Actor* a) {return a&&(a->IsPlayerTeammate()||(follower&&a->IsInFaction(follower)));}
bool valid(RE::Actor* a) {return a&&!a->IsPlayerRef()&&!a->IsDead()&&!a->IsDisabled()&&!a->IsDeleted()&&!a->IsSummoned()&&a->GetActorBase()&&a->Get3D();}
std::vector<RE::ActorHandle> loaded() {std::vector<RE::ActorHandle> out;auto* lists=RE::ProcessLists::GetSingleton();if(lists)lists->ForAllActors([&](RE::Actor* a){if(valid(a))out.push_back(a->GetHandle());return RE::BSContainer::ForEachResult::kContinue;});return out;}
bool member(RE::Actor* a,const std::vector<RE::TESFaction*>& groups) {return std::any_of(groups.begin(),groups.end(),[&](auto* f){return a->IsInFaction(f);});}
bool friends(RE::Actor* a,RE::Actor* b) {auto* rel=RE::BGSRelationship::GetRelationship(a->GetActorBase(),b->GetActorBase());return rel&&unsigned(*rel->level)<=unsigned(RE::BGSRelationship::RELATIONSHIP_LEVEL::kFriend);}
bool shared(RE::Actor* a,RE::Actor* b) {return std::any_of(outlaws.begin(),outlaws.end(),[&](auto* f){return a->IsInFaction(f)&&b->IsInFaction(f);});}
bool sees(RE::Actor* a,RE::Actor* b) {return a->AsActorState()->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kIsSleeping&&a->GetDistance(b)<900&&a->RequestLOS(b)>0&&a->RequestDetectionLevel(b)>0;}
bool nativeCondition(RE::Actor* actor,RE::FUNCTION_DATA::FunctionID function) {
    RE::TESConditionItem condition;condition.data.functionData.function=function;condition.data.comparisonValue.f=1.f;
    RE::ConditionCheckParams params(actor,RE::PlayerCharacter::GetSingleton());return condition.IsTrue(params);
}
void observeOtherBump(const std::vector<RE::ActorHandle>& actors,RE::Actor* victim,RE::PlayerCharacter* player) {
    for(const auto& handle:actors)if(auto observer=handle.get();observer&&observer.get()!=victim&&observer->GetParentCell()==victim->GetParentCell()&&observer->GetDistance(victim)<900){
        // This is personal wariness, not a crime report. Seeing the player and
        // the contact is required; hearing a bump alone is insufficient.
        if(!sees(observer.get(),player)||observer->RequestLOS(victim)<=0||std::abs(observer->GetHeadingAngle(victim->GetPosition(),false))>=90)continue;
        auto& trail=attentionTrails[observer->GetFormID()];witnessBump(trail,playSeconds);
        SKSE::log::info("Bump witness {:08X} watched contact with {:08X}: wariness {:.2f}, next bump effectiveness {:.0f}%",observer->GetFormID(),victim->GetFormID(),trail.wariness,(1.f-.25f*trail.wariness)*100);
    }
}
void observeBumps(const std::vector<RE::ActorHandle>& actors,RE::PlayerCharacter* player) {
    const auto position=player->GetPosition();
    const auto previous=previousPlayerPosition;
    const float step=havePlayerPosition?position.GetDistance(previous):0.f;
    const bool moving=havePlayerPosition&&step>1.f&&step<140.f;
    previousPlayerPosition=position;havePlayerPosition=true;
    std::set<RE::FormID> nearby;
    for(auto& handle:actors)if(auto actor=handle.get();actor&&actor->GetParentCell()==player->GetParentCell()&&actor->GetDistance(player)<180){
        auto* process=actor->GetActorRuntimeData().currentProcess;auto* high=process?process->high:nullptr;if(!high)continue;
        const auto id=actor->GetFormID();nearby.insert(id);
        const auto distance=actor->GetDistance(player);
        // Native small or large bump state remains required. Recent close player motion
        // attributes contact without rejecting every crowded encounter.
        const bool contact=moving&&std::min(distance,actor->GetPosition().GetDistance(previous))<70.f&&actor->AsActorState()->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kIsSleeping;
        auto& observation=bumpObservations[id];
        const auto type=static_cast<int>(high->bumpedState);
        const bool changed=observation.initialized&&(observation.stamp!=high->bumpTimer.timeStamp||observation.type!=type);
        const bool attributed=nativeBump(observation,high->bumpTimer.timeStamp,type,contact,playSeconds);
        const bool accepted=attributed;
        if(changed&&type>=0)SKSE::log::info("Bump observation {:08X}: type {}, distance {:.1f}, player-step {:.1f}, contact {}, recent-contact {}, accepted {}",id,type,distance,step,contact,playSeconds<=observation.contactUntil,accepted);
        if(accepted){
            auto& state=states[id];const auto oldCount=state.bumps;const auto oldUntil=state.bumpUntil;
            auto& trail=attentionTrails[id];
            const float effect=playSeconds-state.lastBump<1?trail.bumpEffect:bumpEffectiveness(trail,playSeconds);
            bump(state,playSeconds,static_cast<unsigned>(type),effect);
            if(state.bumps!=oldCount||state.bumpUntil!=oldUntil){
                if(state.bumpUntil>playSeconds){trail.bumpType=static_cast<unsigned>(type);trail.bumpEffect=effect;}
                SKSE::log::info("Native {} bump {:08X}: count {}, distraction {:.1f}, effectiveness {:.0f}%, window {:.2f}s, suspicion {:.1f}",type==1?"large":"small",id,state.bumps,state.bumpUntil>playSeconds?bumpDistraction(trail.bumpType)*trail.bumpEffect:0.f,effect*100,std::max(0.0,state.bumpUntil-playSeconds),state.suspicion);
            }
            if(state.bumps!=oldCount)observeOtherBump(actors,actor.get(),player);
        }
    }
    std::erase_if(bumpObservations,[&](const auto& entry){return !nearby.contains(entry.first);});
}
Situation reactionFacts(RE::Actor* actor,RE::PlayerCharacter* player) {
    Situation f;
    f.sleeping=actor->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping;
    f.bumpDistracted=!f.sleeping&&states[actor->GetFormID()].bumpUntil>playSeconds;
    f.bumpEffect=attentionTrails[actor->GetFormID()].bumpEffect;
    f.facing=std::abs(actor->GetHeadingAngle(player->GetPosition(),false))<70;
    f.close=actor->GetDistance(player)<65;
    f.detection=std::clamp(float(actor->RequestDetectionLevel(player))/100.f,0.f,1.f);
    f.detected=f.detection>0;
    return f;
}
float weatherNoise(RE::TESWeather* weather) {
    if(!weather||!weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy))return 0;
    bool thunder=weather->data.thunderLightningFrequency>0;
    for(auto* sound:weather->sounds)if(sound&&sound->type.get()==RE::TESWeather::SoundType::kThunder)thunder=true;
    return thunder?12.f:6.f;
}
void environment(RE::Actor* actor,Situation& facts,bool includeWeather=true) {
    facts.guard=actor->IsGuard();if(auto* calendar=RE::Calendar::GetSingleton())facts.hour=calendar->GetHour();
    auto* cell=actor->GetParentCell();facts.outdoors=cell&&!cell->IsInteriorCell();
    if(!includeWeather||!facts.outdoors)return;
    if(auto* sky=RE::Sky::GetSingleton()){
        const float blend=std::clamp(finite(sky->currentWeatherPct,1),0.f,1.f);
        facts.weather=sky->currentWeather?weatherNoise(sky->currentWeather)*blend+weatherNoise(sky->lastWeather)*(1-blend):weatherNoise(sky->lastWeather);
    }
}
Situation situation(RE::Actor* victim,bool detected,const std::vector<RE::ActorHandle>& actors) {
    auto* player=RE::PlayerCharacter::GetSingleton();auto& s=states[victim->GetFormID()];Situation f;environment(victim,f);
    f.failed=s.failedUntil>playSeconds;
    f.attention=25+s.suspicion+(s.failedUntil>playSeconds?25:0)+(s.observedUntil>playSeconds?12:0)+(s.interactionUntil>playSeconds?8:0);
    f.facing=std::abs(victim->GetHeadingAngle(player->GetPosition(),false))<70;
    f.close=victim->GetDistance(player)<65;f.detected=detected;
    f.sleeping=victim->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping;f.sitting=victim->AsActorState()->IsSitting();f.talking=victim->GetCurrentScene()!=nullptr;
    auto* ui=RE::UI::GetSingleton();
    f.speakingToPlayer=ui&&!ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)&&nativeCondition(victim,RE::FUNCTION_DATA::FunctionID::kIsTalking)&&nativeCondition(victim,RE::FUNCTION_DATA::FunctionID::kIsGreetingPlayer);
    if(auto* proc=victim->GetActorRuntimeData().currentProcess) {auto focus=proc->GetHeadtrackTarget().get();f.focusedElsewhere=focus&&focus.get()!=player;}
    f.bumpDistracted=!f.sleeping&&s.bumpUntil>playSeconds;
    f.bumpEffect=attentionTrails[victim->GetFormID()].bumpEffect;
    f.misdirection=(f.bumpDistracted?bumpDistraction(attentionTrails[victim->GetFormID()].bumpType)*f.bumpEffect:0.f)+(s.commotionUntil>playSeconds?10.f:0.f);
    unsigned count=0;
    for(auto& handle:actors) if(auto other=handle.get();other&&other.get()!=victim&&other->GetParentCell()==victim->GetParentCell()&&other->GetDistance(victim)<700) {
        if(following(other.get())||other->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping)continue;
        if(++count>64)break;
        if(other->GetDistance(victim)<250&&!following(other.get()))++f.crowd;
        if(other->IsGuard()||other->IsAlarmed())++f.alerted;
        if(!witnessCounts(following(other.get()),member(other.get(),guilds),member(other.get(),outlaws),friends(other.get(),victim),shared(other.get(),victim)))continue;
        if(!sees(other.get(),player)||other->RequestLOS(victim)<=0)continue;
        auto it=states.find(other->GetFormID());float suspicious=it==states.end()?0:it->second.suspicion;
        Situation observer;observer.attention=25+suspicious;observer.detected=true;
        observer.detection=std::clamp(float(other->RequestDetectionLevel(player))/100.f,0.f,1.f);
        observer.facing=std::abs(other->GetHeadingAngle(player->GetPosition(),false))<70;
        observer.close=other->GetDistance(player)<65;observer.sitting=other->AsActorState()->IsSitting();observer.talking=other->GetCurrentScene()!=nullptr;
        if(auto* process=other->GetActorRuntimeData().currentProcess){auto focus=process->GetHeadtrackTarget().get();observer.focusedElsewhere=focus&&focus.get()!=player;}
        environment(other.get(),observer,false);
        f.witnessPenalty+=std::clamp(attention(observer)*.12f+(other->IsGuard()?4:0),0.f,12.f);
    }
    f.witnessPenalty=std::min(f.witnessPenalty,45.f);return f;
}
std::int32_t chance(float thiefSkill,float targetSkill,std::uint32_t value,float weight,RE::Actor* thief,RE::Actor* target,bool detected,RE::TESForm* item) {
    if(!active||!thief||!thief->IsPlayerRef()||!valid(target))return original(thiefSkill,targetSkill,value,weight,thief,target,detected,item);
    std::scoped_lock lock(mutex);
    auto* ini=RE::INISettingCollection::GetSingleton();auto* setting=ini?ini->GetSetting("fActivatePickLength:Interface"):nullptr;
    const float reach=setting&&std::isfinite(setting->data.f)&&setting->data.f>0?setting->data.f:180.f;
    if(thief->GetDistance(target)>reach*config.distance)return 0;
    auto actors=loaded();auto f=situation(target,detected,actors);f.weight=weight;f.value=value;
    f.detection=std::clamp(float(target->RequestDetectionLevel(thief))/100.f,0.f,1.f);
    f.profileRisk=target->IsGuard()?1.f:member(target,outlaws)?.4f:0.f;
    const auto race=target->GetRace();auto* base=target->GetActorBase();auto* baseRace=base->GetRace();auto* voice=base->GetVoiceType();
    if(!voice)if(auto* face=base->GetRootFaceNPC())voice=face->GetVoiceType();
    const bool elderly=elderRecord(race?race->GetFormID():0,baseRace?baseRace->GetFormID():0,voice?voice->GetFormID():0);
    f.ageRelief=elderly?.75f:0.f;
    f.lingeringReaction=sampleReaction(attentionTrails[target->GetFormID()],f,playSeconds);
    cachedCap=cap(f,config,thiefSkill);float hand=handChance(weight,value,config,thiefSkill);
    // Same argument order as vanilla ComputePickpocketSuccess. Apply perk/item
    // entry points before independent attention/witness penalties and final cap.
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPickpocketChance,thief,target,item,&hand);
    const auto result=finalChance(hand,thiefSkill,f,config);
    // Bounded diagnostic: one record at most per second when selection/state/result
    // changes. No actor pointers or growing per-item diagnostic cache are retained.
    struct Trace {RE::FormID target=0,item=0;bool sleeping=false,bump=false;int result=-1;float witness=-1,profile=-1;Clock::time_point time{};};
    static Trace trace;
    const auto now=Clock::now();const auto itemID=item?item->GetFormID():0;
    if((trace.target!=target->GetFormID()||trace.item!=itemID||trace.sleeping!=f.sleeping||trace.bump!=f.bumpDistracted||trace.result!=result||trace.witness!=config.witness||trace.profile!=config.profile)&&now-trace.time>=std::chrono::seconds(1)){
        SKSE::log::info("Chance target {:08X}, item {:08X}: actor-state-offset {:X}, sleep-state {}, sleeping {}, skill {:.1f}, weight {:.2f}, value {}, witnesses {:.1f}, awareness-loss {:.1f}, sleep-relief {:.1f}, bump-active {}, bump-left {:.2f}, bump-count {}, misdirection {:.1f}, facing {}, detected {}, suspicion {:.1f}, witness-strength {:.1f}, witness-loss {:.1f}, race {:08X}, voice {:08X}, elderly {}, age-relief {:.2f}, occupation-risk {:.2f}, hour {:.2f}, time-attention {:.2f}, guard {}, weather-noise {:.2f}, skill-starting-ceiling {:.2f}, skill-cap-relief {:.2f}, profile-points {:.2f}, lingering-reaction {:.2f}, bump-effectiveness {:.2f}, failed {}, hand {:.1f}, cap {:.1f}, chance {}",target->GetFormID(),itemID,reinterpret_cast<std::uintptr_t>(target->AsActorState())-reinterpret_cast<std::uintptr_t>(target),static_cast<unsigned>(target->AsActorState()->GetSitSleepState()),f.sleeping,thiefSkill,weight,value,f.witnessPenalty,awarenessLoss(f,config),sleepRelief(f,config),f.bumpDistracted,std::max(0.0,states[target->GetFormID()].bumpUntil-playSeconds),states[target->GetFormID()].bumps,f.misdirection,f.facing,f.detected,states[target->GetFormID()].suspicion,config.witness,witnessLoss(f,config),race?race->GetFormID():0,voice?voice->GetFormID():0,elderly,ageBenefit(f,config),f.profileRisk,f.hour,timeAwareness(f.hour,f.guard),f.guard,weatherDistraction(f),skillStartingCeiling(config,thiefSkill),skillCapRelief(f,config,thiefSkill),config.profile,f.lingeringReaction,f.bumpEffect,f.failed,hand,cachedCap.load(),result);
        trace={target->GetFormID(),itemID,f.sleeping,f.bumpDistracted,result,config.witness,config.profile,now};
    }
    return result;
}
template<unsigned Index>
std::int32_t chanceSite(float skill,float targetSkill,std::uint32_t value,float weight,RE::Actor* thief,RE::Actor* target,bool detected,RE::TESForm* item) {
    if(!active||!thief||!thief->IsPlayerRef()||!valid(target))return previousChance[Index](skill,targetSkill,value,weight,thief,target,detected,item);
    return chance(skill,targetSkill,value,weight,thief,target,detected,item);
}
bool executableAddress(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;
}
bool installChance() {
    try {
        REL::Relocation<std::uintptr_t> target{RELOCATION_ID(25822,26379)};original=reinterpret_cast<ChanceFn>(target.address());
        auto& module=REL::Module::get();const auto base=module.base();
        const std::filesystem::path path(module.filePath().data());
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input)throw std::runtime_error("Cannot open running executable");
        const auto size=input.tellg();if(size<=0||size>512*1024*1024)throw std::runtime_error("Invalid executable size");
        std::vector<std::uint8_t> file(static_cast<std::size_t>(size));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(file.data()),static_cast<std::streamsize>(file.size())))throw std::runtime_error("Cannot read executable");
        const auto sites=Hooks::discover(file,target.address()-base);
        if(sites.size()!=2)throw std::runtime_error("Expected exactly two original chance callers");
        const auto text=module.segment(REL::Segment::textx);
        for(unsigned i=0;i<2;++i){const auto address=base+sites[i].rva;
            if(address<text.address()+8||address+13>text.address()+text.size())throw std::runtime_error("Chance caller outside executable text");
            const std::span<const std::uint8_t> live(reinterpret_cast<const std::uint8_t*>(address-8),21);
            if(!Hooks::contextMatches(file,sites[i],live))throw std::runtime_error("Chance caller instruction context changed");
            const auto delegate=static_cast<std::uintptr_t>(Hooks::destination(live,8,address));
            if(!executableAddress(delegate))throw std::runtime_error("Previous chance destination is not executable");
            previousChance[i]=reinterpret_cast<ChanceFn>(delegate);
            SKSE::log::info("Verified chance caller RVA {:X}; {} destination {:X}",sites[i].rva,delegate==target.address()?"original":"redirected",delegate);
        }
        // Validate both locations before modifying either. Separate delegates preserve
        // prior behavior for inactive/non-player calls without double-applying perks.
        SKSE::AllocTrampoline(128);
        auto& trampoline=SKSE::GetTrampoline();
        trampoline.write_call<5>(base+sites[0].rva,chanceSite<0>);
        trampoline.write_call<5>(base+sites[1].rva,chanceSite<1>);
        SKSE::log::info("Installed both shared chance hooks (menu and theft roll); player chances owned by Pickpocket Overhaul");
        return true;
    }catch(const std::exception& e){SKSE::log::error("Chance hook validation failed: {}; overhaul disabled",e.what());return false;}
}
using AttemptFn=bool(*)(RE::PlayerCharacter*,RE::TESObjectREFR*,RE::InventoryEntryData*,std::int32_t,bool);
AttemptFn previousAttempt=nullptr;
bool attempt(RE::PlayerCharacter* player,RE::TESObjectREFR* container,RE::InventoryEntryData* entry,std::int32_t number,bool fromContainer) {
    const auto generation=session.load();
    auto* actor=container?container->As<RE::Actor>():nullptr;
    const bool tracked=active&&player&&player->IsPlayerRef()&&valid(actor)&&entry&&number>0;
    const auto id=tracked?actor->GetFormID():0;
    const bool success=previousAttempt(player,container,entry,number,fromContainer);
    if(tracked&&!success&&active&&session==generation){std::scoped_lock lock(mutex);failedTheft(states[id],playSeconds);SKSE::log::info("Failed theft {:08X}: 80 percent chance reduction for 60 seconds",id);}
    return success;
}
bool installAttempts() {
    try {
        REL::Relocation<std::uintptr_t> target{RELOCATION_ID(39568,40654)};
        auto& module=REL::Module::get();const auto base=module.base();
        std::ifstream input(std::filesystem::path(module.filePath().data()),std::ios::binary|std::ios::ate);
        if(!input)throw std::runtime_error("Cannot open executable");
        const auto size=input.tellg();if(size<=0||size>512*1024*1024)throw std::runtime_error("Invalid executable size");
        std::vector<std::uint8_t> file(static_cast<std::size_t>(size));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(file.data()),static_cast<std::streamsize>(file.size())))throw std::runtime_error("Cannot read executable");
        const auto sites=Hooks::discover(file,target.address()-base);
        if(sites.size()!=1)throw std::runtime_error("Expected exactly one native theft-attempt caller");
        const auto address=base+sites[0].rva;const auto text=module.segment(REL::Segment::textx);
        if(address<text.address()+8||address+13>text.address()+text.size())throw std::runtime_error("Attempt caller outside text");
        const std::span<const std::uint8_t> live(reinterpret_cast<const std::uint8_t*>(address-8),21);
        if(!Hooks::contextMatches(file,sites[0],live))throw std::runtime_error("Attempt caller context changed");
        const auto delegate=static_cast<std::uintptr_t>(Hooks::destination(live,8,address));
        if(!executableAddress(delegate))throw std::runtime_error("Attempt destination not executable");
        previousAttempt=reinterpret_cast<AttemptFn>(delegate);
        SKSE::GetTrampoline().write_call<5>(address,attempt);
        SKSE::log::info("Installed native theft-result hook at RVA {:X}",sites[0].rva);return true;
    }catch(const std::exception& e){SKSE::log::error("Failed-theft result hook unavailable: {}; failure penalties cannot be recorded",e.what());return false;}
}
// Reuse the game's failed-pickpocket alarm, including its crime type, bounty,
// witnesses and response dialogue. No synthetic theft or second fine is added.
bool prepareRecognitionAlarm() {
    try {
        REL::Relocation<std::uintptr_t> alarm{RELOCATION_ID(36428,37423)};
        REL::Relocation<std::uintptr_t> attemptTarget{RELOCATION_ID(39568,40654)};
        auto& module=REL::Module::get();const auto base=module.base();
        std::ifstream input(std::filesystem::path(module.filePath().data()),std::ios::binary|std::ios::ate);
        if(!input)throw std::runtime_error("Cannot open executable");
        const auto size=input.tellg();if(size<=0||size>512*1024*1024)throw std::runtime_error("Invalid executable size");
        std::vector<std::uint8_t> file(static_cast<std::size_t>(size));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(file.data()),static_cast<std::streamsize>(file.size())))throw std::runtime_error("Cannot read executable");
        auto sites=Hooks::discover(file,alarm.address()-base);
        // Require the alarm call used by the native theft attempt. Reject a
        // missing/ambiguous mapping instead of falling back to a value-based fine.
        const auto attemptRva=attemptTarget.address()-base;
        std::erase_if(sites,[&](const auto& site){return site.rva<attemptRva||site.rva-attemptRva>=0x800;});
        if(sites.size()!=1)throw std::runtime_error("Expected one pickpocket-alarm call inside the native theft attempt");
        const auto address=base+sites[0].rva;const auto text=module.segment(REL::Segment::textx);
        if(address<text.address()+8||address+13>text.address()+text.size())throw std::runtime_error("Alarm caller outside text");
        const std::span<const std::uint8_t> live(reinterpret_cast<const std::uint8_t*>(address-8),21);
        if(!Hooks::contextMatches(file,sites[0],live))throw std::runtime_error("Alarm caller context changed");
        const auto destination=static_cast<std::uintptr_t>(Hooks::destination(live,8,address));
        if(!executableAddress(destination))throw std::runtime_error("Alarm destination not executable");
        recognitionAlarm=reinterpret_cast<PickpocketAlarmFn>(destination);
        SKSE::log::info("Recognition uses native failed-pickpocket alarm RVA {:X}, verified caller RVA {:X}; vanilla dialogue and pickpocket bounty",alarm.address()-base,sites[0].rva);
        return true;
    }catch(const std::exception& e){recognitionAlarm=nullptr;SKSE::log::error("Recognition alarm unavailable: {}; recognized-item reporting disabled",e.what());return false;}
}
RE::ExtraDataList* instance(RE::TESObjectREFR* container,const Stolen& s) {
    if(!container)return nullptr;auto inv=container->GetInventory([&](RE::TESBoundObject& o){return o.GetFormID()==s.form;});
    for(auto& [object,e]:inv)if(e.first>0&&e.second&&e.second->extraLists)for(auto* x:*e.second->extraLists)if(x)if(auto* id=x->GetByType<RE::ExtraUniqueID>();id&&id->baseID==s.uidBase&&id->uniqueID==s.uid)return x;
    return nullptr;
}
struct Events final:RE::BSTEventSink<RE::TESContainerChangedEvent>,RE::BSTEventSink<RE::TESUniqueIDChangeEvent>,RE::BSTEventSink<RE::TESHitEvent>,RE::BSTEventSink<RE::MenuOpenCloseEvent>,RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    RE::FormID menuVictim=0;bool transferred=false;
    static Events& get(){static Events e;return e;}
    template<class F>void queue(F f){auto generation=session.load();if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([f=std::move(f),generation]{if(active&&session==generation){std::scoped_lock lock(mutex);f();}});}
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* e,RE::BSTEventSource<RE::TESContainerChangedEvent>*)override {
        if(e&&active){auto copy=*e;queue([this,copy]{
            for(auto& s:stolen)if(s.form==copy.baseObj&&s.container==copy.oldContainer&&s.uid==copy.uniqueID)s.container=copy.newContainer;
            if(copy.newContainer!=0x14||copy.oldContainer!=menuVictim||!menuVictim||copy.itemCount<=0)return;
            transferred=true;auto* victim=RE::TESForm::LookupByID<RE::Actor>(copy.oldContainer);auto* player=RE::PlayerCharacter::GetSingleton();if(!victim||!player)return;
            auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(copy.baseObj);if(!object)return;
            auto inv=player->GetInventory([&](RE::TESBoundObject& o){return o.GetFormID()==copy.baseObj;});RE::ExtraDataList* selected=nullptr;unsigned matches=0;
            for(auto& [o,data]:inv)if(data.second&&data.second->extraLists)for(auto* x:*data.second->extraLists)if(x&&x->GetOwner()==victim->GetActorBase()) {
                auto* uid=x->GetByType<RE::ExtraUniqueID>();
                if(copy.uniqueID?(uid&&uid->uniqueID==copy.uniqueID):(x->GetCount()==copy.itemCount)){selected=x;++matches;}
            }
            if(matches!=1||!selected){SKSE::log::info("Theft {:08X}: ambiguous instance; ownership left unchanged",copy.baseObj);return;}
            auto* uid=selected->GetByType<RE::ExtraUniqueID>();if(!uid){auto* changes=player->GetInventoryChanges();if(!changes)return;selected->Add(new RE::ExtraUniqueID(player->GetFormID(),changes->GetNextUniqueID()));uid=selected->GetByType<RE::ExtraUniqueID>();}if(!uid)return;
            Stolen s;s.victim=victim->GetFormID();s.form=copy.baseObj;s.owner=victim->GetActorBase()->GetFormID();s.container=0x14;s.uidBase=uid->baseID;s.uid=uid->uniqueID;s.value=std::uint32_t(std::max(0,object->GetGoldValue()));auto now=days();s.noticed=now+(20+std::min(s.value,1000u)*.03)/86400;s.expires=now+memoryDays(s.value,config.tagDays);s.recognizesUntil=now+memoryDays(s.value,config.recognitionDays);
            if(std::none_of(stolen.begin(),stolen.end(),[&](const auto& old){return old.form==s.form&&old.uidBase==s.uidBase&&old.uid==s.uid;}))stolen.push_back(s);
        });}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESUniqueIDChangeEvent* e,RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*)override {
        if(e){auto copy=*e;queue([copy]{for(auto& s:stolen)if(s.form==copy.objectID&&s.uidBase==copy.oldBaseID&&s.uid==copy.oldUniqueID){s.uidBase=copy.newBaseID;s.uid=copy.newUniqueID;}});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* e,RE::BSTEventSource<RE::TESHitEvent>*)override {
        if(e&&e->target){auto handle=e->target->GetHandle();queue([handle]{if(auto ref=handle.get())for(auto& h:loaded())if(auto a=h.get();a&&a->GetParentCell()==ref->GetParentCell()&&a->GetDistance(ref.get())<900)states[a->GetFormID()].commotionUntil=playSeconds+8;});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* e,RE::BSTEventSource<RE::MenuOpenCloseEvent>*)override {
        if(e&&e->menuName==RE::ContainerMenu::MENU_NAME){auto open=e->opening;queue([this,open]{if(open&&RE::ContainerMenu::GetContainerMode()==RE::ContainerMenu::ContainerMode::kPickpocket){auto ref=RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());auto* a=ref?ref->As<RE::Actor>():nullptr;menuVictim=a?a->GetFormID():0;transferred=false;if(menuVictim){auto& s=states[menuVictim];auto* player=RE::PlayerCharacter::GetSingleton();bool observed=player&&s.bumpUntil<=playSeconds&&sees(a,player)&&std::abs(a->GetHeadingAngle(player->GetPosition(),false))<70;if(auto* process=a->GetActorRuntimeData().currentProcess){auto focus=process->GetHeadtrackTarget().get();if(focus&&focus.get()!=player)observed=false;}interaction(s,playSeconds,observed);SKSE::log::info("Inventory interaction {:08X}: observed {}, suspicion {:.1f}",menuVictim,observed,s.suspicion);}}else if(!open){menuVictim=0;}});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* e,RE::BSTEventSource<RE::BSAnimationGraphEvent>*)override {
        if(e&&e->holder){const auto id=e->holder->GetFormID();const std::string tag=e->tag.c_str();queue([id,tag]{auto* a=RE::TESForm::LookupByID<RE::Actor>(id);auto* p=RE::PlayerCharacter::GetSingleton();if(!valid(a)||!p)return;if(tag=="SoundPlay.NPCHumanEat"||tag=="SoundPlay.NPCHumanDrink")states[id].commotionUntil=playSeconds+5;});}return RE::BSEventNotifyControl::kContinue;
    }
};
void tick() {
    auto now=Clock::now();float delta=std::chrono::duration<float>(now-lastTick).count();lastTick=now;
    auto* ui=RE::UI::GetSingleton();if(!active||!ui||ui->GameIsPaused())return;
    std::scoped_lock lock(mutex);playSeconds+=std::clamp(delta,0.f,2.f);auto actors=loaded();auto* p=RE::PlayerCharacter::GetSingleton();if(!p)return;
    observeBumps(actors,p);if(playSeconds<nextFullTick)return;nextFullTick=playSeconds+.5;
    for(auto& h:actors)if(auto a=h.get();a&&a->GetParentCell()==p->GetParentCell()&&a->GetDistance(p)<1200){auto& s=states[a->GetFormID()];decay(s,playSeconds);sampleReaction(attentionTrails[a->GetFormID()],reactionFacts(a.get(),p),playSeconds);if(a->IsInCombat())s.commotionUntil=playSeconds+4;if(s.bumpUntil<=playSeconds&&p->IsSneaking()&&sees(a.get(),p))s.observedUntil=playSeconds+30;if(animated.insert(a->GetFormID()).second&&!a->AddAnimationGraphEventSink(&Events::get()))animated.erase(a->GetFormID());}
    double today=days();std::set<std::pair<std::uint32_t,std::uint16_t>> completed;std::set<RE::FormID> reportedVictims;
    for(auto& s:stolen){auto* container=RE::TESForm::LookupByID<RE::TESObjectREFR>(s.container);if(!container)continue;auto* x=instance(container,s);if(!x)continue;
        if(today>=std::max(s.expires,s.recognizesUntil))completed.emplace(s.uidBase,s.uid);
        if(today>=s.noticed&&today<s.recognizesUntil)states[s.victim].suspicion=std::max(15.f,states[s.victim].suspicion);
        if(today>=s.expires&&x->GetOwner()&&x->GetOwner()->GetFormID()==s.owner)x->SetOwner(p->GetActorBase());
        if(s.reported||today<s.noticed||today>=s.recognizesUntil||s.container!=0x14||!(x->HasType<RE::ExtraWorn>()||x->HasType<RE::ExtraWornLeft>()))continue;
        auto* victim=RE::TESForm::LookupByID<RE::Actor>(s.victim);if(!valid(victim)||following(victim)||!sees(victim,p))continue;
        if(!recognitionAlarm)continue;
        s.reported=true;states[s.victim].suspicion=60;
        // Several worn possessions recognized together are one confrontation.
        if(reportedVictims.insert(s.victim).second){
            auto* faction=victim->GetCrimeFaction();const auto before=faction?faction->GetCrimeGoldNonViolent():0;
            // Match the vanilla failed-attempt call exactly: player, victim,
            // no transferred object, count zero. The engine selects crime 1.
            recognitionAlarm(p,victim,nullptr,0);
            const auto after=faction?faction->GetCrimeGoldNonViolent():0;
            SKSE::log::info("Victim {:08X} recognized stolen item {:08X}: native pickpocket reaction, crime faction {:08X}, nonviolent bounty {} -> {}",s.victim,s.form,faction?faction->GetFormID():0,before,after);
        }else SKSE::log::info("Victim {:08X} also recognized {:08X}; same confrontation, no second alarm",s.victim,s.form);
    }
    std::erase_if(stolen,[&](const auto& s){return completed.contains({s.uidBase,s.uid});});
}
constexpr std::uint32_t record=0x504F5331,trailRecord=0x504F5431;
void save(SKSE::SerializationInterface* api){std::scoped_lock lock(mutex);if(!api->OpenRecord(record,1))return;api->WriteRecordData(&playSeconds,sizeof(playSeconds));auto count=std::uint32_t(states.size());api->WriteRecordData(&count,4);for(auto& [id,s]:states){api->WriteRecordData(&id,4);api->WriteRecordData(&s,sizeof(s));}count=std::uint32_t(stolen.size());api->WriteRecordData(&count,4);for(auto& s:stolen)api->WriteRecordData(&s,sizeof(s));
if(api->OpenRecord(trailRecord,3)){count=0;for(const auto& [id,trail]:attentionTrails)if(trail.reaction>0||trail.wariness>0||(states.contains(id)&&states.at(id).bumpUntil>playSeconds))++count;api->WriteRecordData(&count,4);for(const auto& [id,trail]:attentionTrails)if(trail.reaction>0||trail.wariness>0||(states.contains(id)&&states.at(id).bumpUntil>playSeconds)){api->WriteRecordData(&id,4);api->WriteRecordData(&trail.last,sizeof(trail.last));api->WriteRecordData(&trail.reaction,sizeof(trail.reaction));api->WriteRecordData(&trail.bumpType,sizeof(trail.bumpType));api->WriteRecordData(&trail.bumpEffect,sizeof(trail.bumpEffect));api->WriteRecordData(&trail.wariness,sizeof(trail.wariness));api->WriteRecordData(&trail.warinessLast,sizeof(trail.warinessLast));}}}
void clear(SKSE::SerializationInterface*){active=false;++session;std::scoped_lock lock(mutex);states.clear();attentionTrails.clear();stolen.clear();animated.clear();bumpObservations.clear();havePlayerPosition=false;nextFullTick=0;playSeconds=0;cachedCap=-1;Events::get().menuVictim=0;}
void load(SKSE::SerializationInterface* api){clear(api);std::uint32_t type,version,length;while(api->GetNextRecordInfo(type,version,length)){if(type==trailRecord&&(version>=1&&version<=3)){std::uint32_t count;if(api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){std::uint32_t id,resolved;AttentionTrail trail;if(api->ReadRecordData(&id,4)!=4||api->ReadRecordData(&trail.last,sizeof(trail.last))!=sizeof(trail.last)||api->ReadRecordData(&trail.reaction,sizeof(trail.reaction))!=sizeof(trail.reaction))return;if(version>=2&&api->ReadRecordData(&trail.bumpType,sizeof(trail.bumpType))!=sizeof(trail.bumpType))return;if(version>=3&&(api->ReadRecordData(&trail.bumpEffect,sizeof(trail.bumpEffect))!=sizeof(trail.bumpEffect)||api->ReadRecordData(&trail.wariness,sizeof(trail.wariness))!=sizeof(trail.wariness)||api->ReadRecordData(&trail.warinessLast,sizeof(trail.warinessLast))!=sizeof(trail.warinessLast)))return;if(api->ResolveFormID(id,resolved)&&std::isfinite(trail.bumpEffect)&&trail.bumpEffect>=.25f&&trail.bumpEffect<=1&&std::isfinite(trail.wariness)&&trail.wariness>=0&&trail.wariness<=3&&std::isfinite(trail.warinessLast)&&trail.bumpType<=1&&std::isfinite(trail.last)&&std::isfinite(trail.reaction)&&trail.reaction>=0&&trail.reaction<=57)attentionTrails[resolved]=trail;}continue;}if(type!=record||version!=1)continue;std::uint32_t count;if(api->ReadRecordData(&playSeconds,sizeof(playSeconds))!=sizeof(playSeconds)||!std::isfinite(playSeconds)||api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){std::uint32_t id,newID;AttentionState s;if(api->ReadRecordData(&id,4)!=4||api->ReadRecordData(&s,sizeof(s))!=sizeof(s))return;if(api->ResolveFormID(id,newID)&&std::isfinite(s.suspicion))states.emplace(newID,s);}if(api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){Stolen s;if(api->ReadRecordData(&s,sizeof(s))!=sizeof(s))return;bool validIDs=true;for(auto* id:{&s.victim,&s.form,&s.owner,&s.container,&s.uidBase}){std::uint32_t next=0;if(*id&&!api->ResolveFormID(*id,next)){validIDs=false;break;}if(*id)*id=next;}if(validIDs&&std::isfinite(s.expires)&&std::isfinite(s.recognizesUntil))stolen.push_back(s);}}}
void message(SKSE::MessagingInterface::Message* e){switch(e->type){case SKSE::MessagingInterface::kPostLoad:{auto h=GetModuleHandleW(L"StandingPickpocket.dll");if(h){auto request=reinterpret_cast<StandingPickpocketAPI::Request>(GetProcAddress(h,"StandingPickpocket_RequestAPI"));if(request)standing=request(1);}distanceOverride();break;}
case SKSE::MessagingInterface::kDataLoaded:{auto* data=RE::TESDataHandler::GetSingleton();auto faction=[data](unsigned id){return data->LookupForm<RE::TESFaction>(id,"Skyrim.esm");};follower=faction(0x5C84E);for(unsigned id:{0x29DA9u,0x1F259u,0x48362u,0x1BDB3u})if(auto* f=faction(id))guilds.push_back(f);for(unsigned id:{0x1BCC0u,0x60028u,0x29DA9u})if(auto* f=faction(id))outlaws.push_back(f);auto* source=RE::ScriptEventSourceHolder::GetSingleton();auto& events=Events::get();source->AddEventSink<RE::TESContainerChangedEvent>(&events);source->AddEventSink<RE::TESUniqueIDChangeEvent>(&events);source->AddEventSink<RE::TESHitEvent>(&events);RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);hookReady=installChance();if(hookReady){installAttempts();prepareRecognitionAlarm();}SKSE::log::info("Native small/large bump monitor enabled at 100ms; greetings and sleeping handling enabled");worker=std::jthread([](std::stop_token stop){while(!stop.stop_requested()){std::this_thread::sleep_for(std::chrono::milliseconds(100));if(active&&!queued.exchange(true)){auto generation=session.load();SKSE::GetTaskInterface()->AddTask([generation]{queued=false;if(active&&session==generation)tick();});}}});break;}
case SKSE::MessagingInterface::kPreLoadGame:active=false;++session;break;
case SKSE::MessagingInterface::kNewGame:clear(nullptr);[[fallthrough]];
case SKSE::MessagingInterface::kPostLoadGame:if(e->type==SKSE::MessagingInterface::kPostLoadGame&&!e->data)break;lastTick=Clock::now();active=hookReady.load();distanceOverride();break;default:break;}}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse){SKSE::Init(skse);auto folder=SKSE::log::log_directory();if(folder){*folder/="PickpocketOverhaul.log";auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(folder->string(),true);auto logger=std::make_shared<spdlog::logger>("global",sink);spdlog::set_default_logger(logger);spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);}SKSE::log::info("Pickpocket Overhaul 0.1.18 development build; runtime {}",REL::Module::get().version().string());for(int i=0;i<10;++i){wchar_t text[64];auto fallback=std::to_wstring(*option(i));GetPrivateProfileStringW(L"Settings",keys[i],fallback.c_str(),text,64,configPath);wchar_t* end;float v=std::wcstof(text,&end);if(end!=text)*option(i)=v;}normalize(config);auto* api=SKSE::GetSerializationInterface();api->SetUniqueID(0x504F5631);api->SetSaveCallback(save);api->SetLoadCallback(load);api->SetRevertCallback(clear);if(!SKSE::GetPapyrusInterface()->Register(registerPapyrus))return false;return SKSE::GetMessagingInterface()->RegisterListener(message);}

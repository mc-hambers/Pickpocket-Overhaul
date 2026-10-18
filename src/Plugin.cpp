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
bool sees(RE::Actor* a,RE::Actor* b) {return a->GetDistance(b)<900&&a->RequestLOS(b)>0&&a->RequestDetectionLevel(b)>0;}
bool nativeCondition(RE::Actor* actor,RE::FUNCTION_DATA::FunctionID function) {
    RE::TESConditionItem condition;condition.data.functionData.function=function;condition.data.comparisonValue.f=1.f;
    RE::ConditionCheckParams params(actor,RE::PlayerCharacter::GetSingleton());return condition.IsTrue(params);
}
void observeBumps(const std::vector<RE::ActorHandle>& actors,RE::PlayerCharacter* player) {
    const auto position=player->GetPosition();
    const bool moving=havePlayerPosition&&position.GetDistance(previousPlayerPosition)>1.f;
    previousPlayerPosition=position;havePlayerPosition=true;
    std::set<RE::FormID> nearby;
    for(auto& handle:actors)if(auto actor=handle.get();actor&&actor->GetParentCell()==player->GetParentCell()&&actor->GetDistance(player)<180){
        auto* process=actor->GetActorRuntimeData().currentProcess;auto* high=process?process->high:nullptr;if(!high)continue;
        const auto id=actor->GetFormID();nearby.insert(id);
        const auto distance=actor->GetDistance(player);bool contact=moving&&distance<90&&!actor->IsInCombat()&&actor->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kIsSleeping;
        // The engine exposes bump state, but not its instigator here. Reject an
        // ambiguous crowd contact when another actor is closer to this NPC.
        if(contact)for(auto& otherHandle:actors)if(auto other=otherHandle.get();other&&other.get()!=actor.get()&&other->GetParentCell()==actor->GetParentCell()&&other->GetDistance(actor.get())<distance){contact=false;break;}
        if(nativeBump(bumpObservations[id],high->bumpTimer.timeStamp,static_cast<int>(high->bumpedState),contact,playSeconds)&&nativeCondition(actor.get(),RE::FUNCTION_DATA::FunctionID::kIsSmallBump)){
            auto& state=states[id];const auto oldCount=state.bumps;bump(state,playSeconds);
            if(state.bumps!=oldCount)SKSE::log::info("Native small bump {:08X}: count {}, distraction until {:.2f}, suspicion {:.1f}",id,state.bumps,state.bumpUntil,state.suspicion);
        }
    }
    std::erase_if(bumpObservations,[&](const auto& entry){return !nearby.contains(entry.first);});
}
Situation situation(RE::Actor* victim,bool detected,const std::vector<RE::ActorHandle>& actors) {
    auto* player=RE::PlayerCharacter::GetSingleton();auto& s=states[victim->GetFormID()];Situation f;
    f.attention=25+s.suspicion+(s.failedUntil>playSeconds?25:0)+(s.observedUntil>playSeconds?12:0)+(s.interactionUntil>playSeconds?8:0);
    f.facing=std::abs(victim->GetHeadingAngle(player->GetPosition(),false))<70;
    f.close=victim->GetDistance(player)<65;f.detected=detected;
    f.sleeping=victim->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping;f.sitting=victim->IsSitting();f.talking=victim->GetCurrentScene()!=nullptr;
    auto* ui=RE::UI::GetSingleton();
    f.speakingToPlayer=ui&&!ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)&&nativeCondition(victim,RE::FUNCTION_DATA::FunctionID::kIsTalking)&&nativeCondition(victim,RE::FUNCTION_DATA::FunctionID::kIsGreetingPlayer);
    if(auto* proc=victim->GetActorRuntimeData().currentProcess) {auto focus=proc->GetHeadtrackTarget().get();f.focusedElsewhere=focus&&focus.get()!=player;}
    f.misdirection=(s.bumpUntil>playSeconds?24.f:0.f)+(s.commotionUntil>playSeconds?10.f:0.f);
    unsigned count=0;
    for(auto& handle:actors) if(auto other=handle.get();other&&other.get()!=victim&&other->GetParentCell()==victim->GetParentCell()&&other->GetDistance(victim)<700) {
        if(following(other.get()))continue;
        if(++count>64)break;
        if(other->GetDistance(victim)<250&&!following(other.get()))++f.crowd;
        if(other->IsGuard()||other->IsAlarmed())++f.alerted;
        if(!witnessCounts(following(other.get()),member(other.get(),guilds),member(other.get(),outlaws),friends(other.get(),victim),shared(other.get(),victim)))continue;
        if(!sees(other.get(),player)||other->RequestLOS(victim)<=0)continue;
        auto it=states.find(other->GetFormID());float suspicious=it==states.end()?0:it->second.suspicion;
        f.witnessPenalty+=std::clamp(3.f+suspicious*.12f+(other->IsGuard()?4:0),2.f,12.f);
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
    const auto race=target->GetRace();if(race&&(race->GetFormID()==0x67CD8||race->GetFormID()==0xA82BA))f.profileRisk=std::max(-1.f,f.profileRisk-.75f);
    cachedCap=cap(f,config);float hand=handChance(weight,value,config,thiefSkill);
    // Same argument order as vanilla ComputePickpocketSuccess. Apply perk/item
    // entry points before independent attention/witness penalties and final cap.
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPickpocketChance,thief,target,item,&hand);
    return finalChance(hand,thiefSkill,f,config);
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
        if(e&&e->menuName==RE::ContainerMenu::MENU_NAME){auto open=e->opening;queue([this,open]{if(open&&RE::ContainerMenu::GetContainerMode()==RE::ContainerMenu::ContainerMode::kPickpocket){auto ref=RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());auto* a=ref?ref->As<RE::Actor>():nullptr;menuVictim=a?a->GetFormID():0;transferred=false;if(menuVictim){auto& s=states[menuVictim];if(s.interactionUntil>playSeconds)s.suspicion=std::min(60.f,s.suspicion+5);s.interactionUntil=playSeconds+15;}}else if(!open){if(menuVictim&&!transferred)if(auto* a=RE::TESForm::LookupByID<RE::Actor>(menuVictim);a&&(a->IsAlarmed()||a->IsInCombat())){states[menuVictim].failedUntil=playSeconds+60;}menuVictim=0;}});}return RE::BSEventNotifyControl::kContinue;
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
    for(auto& h:actors)if(auto a=h.get();a&&a->GetDistance(p)<1200){auto& s=states[a->GetFormID()];decay(s,playSeconds);if(a->IsInCombat())s.commotionUntil=playSeconds+4;if(p->IsSneaking()&&sees(a.get(),p))s.observedUntil=playSeconds+30;if(animated.insert(a->GetFormID()).second&&!a->AddAnimationGraphEventSink(&Events::get()))animated.erase(a->GetFormID());}
    double today=days();std::set<std::pair<std::uint32_t,std::uint16_t>> completed;
    for(auto& s:stolen){auto* container=RE::TESForm::LookupByID<RE::TESObjectREFR>(s.container);if(!container)continue;auto* x=instance(container,s);if(!x)continue;
        if(today>=std::max(s.expires,s.recognizesUntil))completed.emplace(s.uidBase,s.uid);
        if(today>=s.noticed&&today<s.recognizesUntil)states[s.victim].suspicion=std::max(15.f,states[s.victim].suspicion);
        if(today>=s.expires&&x->GetOwner()&&x->GetOwner()->GetFormID()==s.owner)x->SetOwner(p->GetActorBase());
        if(s.reported||today<s.noticed||today>=s.recognizesUntil||s.container!=0x14||!(x->HasType<RE::ExtraWorn>()||x->HasType<RE::ExtraWornLeft>()))continue;
        auto* victim=RE::TESForm::LookupByID<RE::Actor>(s.victim);if(!valid(victim)||following(victim)||!sees(victim,p))continue;
        auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(s.form);auto* owner=RE::TESForm::LookupByID(s.owner);if(!object||!owner)continue;
        s.reported=true;states[s.victim].suspicion=60;p->StealAlarm(victim,object,1,int(std::min(s.value,100000u)),owner,false);SKSE::log::info("Victim {:08X} recognized stolen item {:08X}",s.victim,s.form);
    }
    std::erase_if(stolen,[&](const auto& s){return completed.contains({s.uidBase,s.uid});});
}
constexpr std::uint32_t record=0x504F5331;
void save(SKSE::SerializationInterface* api){std::scoped_lock lock(mutex);if(!api->OpenRecord(record,1))return;api->WriteRecordData(&playSeconds,sizeof(playSeconds));auto count=std::uint32_t(states.size());api->WriteRecordData(&count,4);for(auto& [id,s]:states){api->WriteRecordData(&id,4);api->WriteRecordData(&s,sizeof(s));}count=std::uint32_t(stolen.size());api->WriteRecordData(&count,4);for(auto& s:stolen)api->WriteRecordData(&s,sizeof(s));}
void clear(SKSE::SerializationInterface*){active=false;++session;std::scoped_lock lock(mutex);states.clear();stolen.clear();animated.clear();bumpObservations.clear();havePlayerPosition=false;nextFullTick=0;playSeconds=0;cachedCap=-1;Events::get().menuVictim=0;}
void load(SKSE::SerializationInterface* api){clear(api);std::uint32_t type,version,length;while(api->GetNextRecordInfo(type,version,length)){if(type!=record||version!=1)continue;std::uint32_t count;if(api->ReadRecordData(&playSeconds,sizeof(playSeconds))!=sizeof(playSeconds)||!std::isfinite(playSeconds)||api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){std::uint32_t id,newID;AttentionState s;if(api->ReadRecordData(&id,4)!=4||api->ReadRecordData(&s,sizeof(s))!=sizeof(s))return;if(api->ResolveFormID(id,newID)&&std::isfinite(s.suspicion))states.emplace(newID,s);}if(api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){Stolen s;if(api->ReadRecordData(&s,sizeof(s))!=sizeof(s))return;bool validIDs=true;for(auto* id:{&s.victim,&s.form,&s.owner,&s.container,&s.uidBase}){std::uint32_t next=0;if(*id&&!api->ResolveFormID(*id,next)){validIDs=false;break;}if(*id)*id=next;}if(validIDs&&std::isfinite(s.expires)&&std::isfinite(s.recognizesUntil))stolen.push_back(s);}}}
void message(SKSE::MessagingInterface::Message* e){switch(e->type){case SKSE::MessagingInterface::kPostLoad:{auto h=GetModuleHandleW(L"StandingPickpocket.dll");if(h){auto request=reinterpret_cast<StandingPickpocketAPI::Request>(GetProcAddress(h,"StandingPickpocket_RequestAPI"));if(request)standing=request(1);}distanceOverride();break;}
case SKSE::MessagingInterface::kDataLoaded:{auto* data=RE::TESDataHandler::GetSingleton();auto faction=[data](unsigned id){return data->LookupForm<RE::TESFaction>(id,"Skyrim.esm");};follower=faction(0x5C84E);for(unsigned id:{0x29DA9u,0x1F259u,0x48362u,0x1BDB3u})if(auto* f=faction(id))guilds.push_back(f);for(unsigned id:{0x1BCC0u,0x60028u,0x29DA9u})if(auto* f=faction(id))outlaws.push_back(f);auto* source=RE::ScriptEventSourceHolder::GetSingleton();auto& events=Events::get();source->AddEventSink<RE::TESContainerChangedEvent>(&events);source->AddEventSink<RE::TESUniqueIDChangeEvent>(&events);source->AddEventSink<RE::TESHitEvent>(&events);RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);hookReady=installChance();SKSE::log::info("Native small-bump monitor enabled at 100ms; greetings and sleeping handling enabled");worker=std::jthread([](std::stop_token stop){while(!stop.stop_requested()){std::this_thread::sleep_for(std::chrono::milliseconds(100));if(active&&!queued.exchange(true)){auto generation=session.load();SKSE::GetTaskInterface()->AddTask([generation]{queued=false;if(active&&session==generation)tick();});}}});break;}
case SKSE::MessagingInterface::kPreLoadGame:active=false;++session;break;
case SKSE::MessagingInterface::kNewGame:clear(nullptr);[[fallthrough]];
case SKSE::MessagingInterface::kPostLoadGame:if(e->type==SKSE::MessagingInterface::kPostLoadGame&&!e->data)break;lastTick=Clock::now();active=hookReady.load();distanceOverride();break;default:break;}}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse){SKSE::Init(skse);auto folder=SKSE::log::log_directory();if(folder){*folder/="PickpocketOverhaul.log";auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(folder->string(),true);auto logger=std::make_shared<spdlog::logger>("global",sink);spdlog::set_default_logger(logger);spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);}SKSE::log::info("Pickpocket Overhaul 0.1.3 development build; runtime {}",REL::Module::get().version().string());for(int i=0;i<10;++i){wchar_t text[64];auto fallback=std::to_wstring(*option(i));GetPrivateProfileStringW(L"Settings",keys[i],fallback.c_str(),text,64,configPath);wchar_t* end;float v=std::wcstof(text,&end);if(end!=text)*option(i)=v;}normalize(config);auto* api=SKSE::GetSerializationInterface();api->SetUniqueID(0x504F5631);api->SetSaveCallback(save);api->SetLoadCallback(load);api->SetRevertCallback(clear);if(!SKSE::GetPapyrusInterface()->Register(registerPapyrus))return false;return SKSE::GetMessagingInterface()->RegisterListener(message);}

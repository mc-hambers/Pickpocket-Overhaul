// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. NO WARRANTY; see LICENSE.txt.
#include "Core.h"
#include "GuardRules.h"
#include "Contraband.h"
#include "LootCycleAPI.h"
#include "Commotion.h"
#include "Compatibility.h"
#include "Karma.h"
#include "BumpRules.h"
#include "EquippedDrops.h"
#include "GeneratedIni.h"
#include "ChanceSites.h"
#include "StandingPickpocketAPI.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <random>
#include <thread>
#include <spdlog/sinks/basic_file_sink.h>
namespace {
using namespace PO;using Clock=std::chrono::steady_clock;
Config config;std::recursive_mutex mutex;std::map<RE::FormID,AttentionState> states;
std::map<RE::FormID,AttentionTrail> attentionTrails;
std::map<RE::FormID,TheftPressure> theftPressure;
TransferEvidence pendingTransfer;std::uint64_t pendingTicket=0,nextTicket=0;
std::mt19937 pressureRandom{static_cast<std::uint32_t>(Clock::now().time_since_epoch().count())};
std::vector<Stolen> stolen;std::atomic_bool active=false,queued=false;std::atomic_uint64_t session=0;
double playSeconds=0;Clock::time_point lastTick=Clock::now();std::jthread worker;
const StandingPickpocketAPI::V1* standing=nullptr;int ownerToken;
RE::TESFaction* follower=nullptr;std::vector<RE::TESFaction*> guilds,outlaws,outlawLoyalty,imperials,stormcloaks;
std::set<RE::FormID> argumentScenes;
std::map<RE::FormID,Commotion> commotions;
std::set<RE::FormID> fleeingActors;
std::set<RE::FormID> animated;std::map<RE::FormID,BumpObservation> bumpObservations;
RE::NiPoint3 previousPlayerPosition{};bool havePlayerPosition=false;double nextFullTick=0;std::atomic_bool hookReady=false;std::atomic<float> cachedCap=-1;std::atomic_bool distanceOwned=false;
constexpr auto configPath=L"Data/SKSE/Plugins/PickpocketOverhaul.ini";
using ChanceFn=std::int32_t(*)(float,float,std::uint32_t,float,RE::Actor*,RE::Actor*,bool,RE::TESForm*);
ChanceFn original=nullptr;
ChanceFn previousChance[2]{};
using PickpocketAlarmFn=void(*)(RE::Actor*,RE::TESObjectREFR*,RE::TESForm*,std::int32_t);
PickpocketAlarmFn recognitionAlarm=nullptr;
struct WitnessDisplay {std::uint32_t targetHandle=0;int chance=0;float effect=0;bool valid=false;};
WitnessDisplay witnessDisplayCache;
std::atomic_bool debugMode=false;
std::atomic_bool forceChance100=false;

std::atomic_bool stolenTracking=true,stolenTagExpiration=true;
std::atomic_bool openingPenalty=true;
std::atomic_int timerPreview=-1;
void setTimerPreview(RE::StaticFunctionTag*,int index) {
    const int next=index==1||index==3?index:-1;
    const auto old=timerPreview.exchange(next);
    if(old!=next)SKSE::log::info("MCM timer hover: {}",next);
}
std::atomic_bool karmaMode=true,karmaReady=false;
BadLuckProtection karma;
std::atomic_bool equippedDrops=false,equippedDropsReady=false;
std::vector<DropRecovery> dropRecoveries;
std::set<RE::FormID> reportedGroundPickups;
struct DeferredDrop {RE::FormID victim=0,form=0,uidBase=0;std::uint16_t uid=0;double after=0;};
std::vector<DeferredDrop> deferredDrops;
double nextRecovery=0;
struct RecoveryPath {RE::ObjectRefHandle target,goal;std::uint64_t generation=0;std::string ticket;double sent=0;bool claimed=false,finished=false,reached=false,started=false,walkRequired=true,walkPrepared=false,walking=false;unsigned dispatches=0,failures=0;double retryAfter=0,lastPickup=-100,lastGateLog=-100,readyAt=0;};
std::map<RE::FormID,RecoveryPath> recoveryPaths;
std::map<RE::FormID,RecoveryProgress> recoveryProgress;
RE::TESPackage* recoveryPausePackage=nullptr;
struct RecoveryPause {std::uintptr_t package=0;double until=0;};
std::map<RE::FormID,RecoveryPause> recoveryPauses;
void releaseRecoveryPause(RE::Actor* actor) {
    if(!actor)return;auto it=recoveryPauses.find(actor->GetFormID());if(it==recoveryPauses.end())return;
    auto* current=actor->GetCurrentPackage();
    if(current&&reinterpret_cast<std::uintptr_t>(current)==it->second.package&&current->packData.packType==RE::PACKAGE_TYPE::kDoNothing)actor->EndInterruptPackage(false);
    recoveryPauses.erase(it);
}
void pauseRecovery(RE::Actor* actor,double duration) {
    if(!actor||actor->IsInCombat())return;
    if(auto existing=recoveryPauses.find(actor->GetFormID());existing!=recoveryPauses.end()){
        if(actor->GetCurrentPackage()==recoveryPausePackage){existing->second.until=playSeconds+duration;return;}
        recoveryPauses.erase(existing);
    }
    if(!recoveryPausePackage)return;
    auto* previous=actor->GetCurrentPackage();actor->PutCreatedPackage(recoveryPausePackage,false,false,false);auto* current=actor->GetCurrentPackage();
    if(current&&current!=previous&&current->packData.packType==RE::PACKAGE_TYPE::kDoNothing)recoveryPauses[actor->GetFormID()]={reinterpret_cast<std::uintptr_t>(current),playSeconds+duration};
}
void serviceRecoveryPauses() {
    std::vector<RE::FormID> finished;
    for(const auto& [id,pause]:recoveryPauses){auto* actor=RE::TESForm::LookupByID<RE::Actor>(id);if(!actor){finished.push_back(id);continue;}if(!actor->Is3DLoaded())continue;if(actor->IsInCombat()||playSeconds>=pause.until)finished.push_back(id);}
    for(auto id:finished){releaseRecoveryPause(RE::TESForm::LookupByID<RE::Actor>(id));recoveryPauses.erase(id);}
}
struct EquipRetry {double after=0;unsigned attempts=0;};
std::map<RE::FormID,EquipRetry> equipRetries;
std::set<RE::FormID> recoverySounds;
struct RetiredGoal {RE::FormID reference=0,owner=0;double after=0;};
std::vector<RetiredGoal> retiredGoals;
void cancelRecoveryWalk(RE::Actor* actor,RE::TESObjectREFR* goal) {
    if(!actor||!goal||!actor->Is3DLoaded())return;
    auto* process=actor->GetActorRuntimeData().currentProcess;if(!process||!process->middleHigh)return;
    bool owned=false;
    {auto& running=process->middleHigh->runOncePackage;RE::BSSpinLockGuard lock(running.packageLock);owned=running.package&&running.target==goal->GetHandle();}
    if(owned){actor->EndInterruptPackage(false);SKSE::log::info("Recovery walk released {:08X}, goal {:08X}; ordinary AI resumes",actor->GetFormID(),goal->GetFormID());}
}
void retireRecoveryGoal(RE::TESObjectREFR* goal,RE::FormID owner=0) {
    if(!goal||!goal->IsDynamicForm()||!goal->GetBaseObject()||goal->GetBaseObject()->GetFormID()!=0x3B)return;
    if(!owner)for(const auto& [id,path]:recoveryPaths)if(auto ref=path.goal.get();ref&&ref.get()==goal){owner=id;break;}
    if(!owner)return; // do not delete an unidentified old callback's goal
    const auto id=goal->GetFormID();
    if(std::none_of(retiredGoals.begin(),retiredGoals.end(),[&](const RetiredGoal& old){return old.reference==id;}))retiredGoals.push_back({id,owner,playSeconds+30});
}
void reapRecoveryGoals() {
    std::erase_if(retiredGoals,[&](const RetiredGoal& old){
        if(playSeconds<old.after)return false;
        for(const auto& [id,path]:recoveryPaths)if(auto activeGoal=path.goal.get();activeGoal&&activeGoal->GetFormID()==old.reference&&!path.finished)return false;
        auto* goal=RE::TESForm::LookupByID<RE::TESObjectREFR>(old.reference);if(!goal||goal->IsDeleted())return true;
        auto* owner=RE::TESForm::LookupByID<RE::Actor>(old.owner);
        if(owner&&(!owner->Is3DLoaded()||owner->IsInCombat()||owner->IsPathing()))return false;
        if(goal->IsDynamicForm()&&goal->GetBaseObject()&&goal->GetBaseObject()->GetFormID()==0x3B)goal->SetDelete(true);
        return true;
    });
}

RE::TESObjectREFR* beginRecovery(RE::StaticFunctionTag*,RE::Actor* owner,RE::BSFixedString ticket) {
    if(!owner)return nullptr;std::scoped_lock lock(mutex);
    auto it=recoveryPaths.find(owner->GetFormID());
    if(it==recoveryPaths.end()||it->second.ticket!=ticket.c_str()||it->second.generation!=session.load()||it->second.claimed||!active)return nullptr;
    it->second.claimed=true;it->second.started=true;return it->second.goal.get().get();
}
void finishRecovery(RE::StaticFunctionTag*,RE::Actor* owner,RE::BSFixedString ticket,bool reached) {
    if(!owner)return;const auto id=owner->GetFormID();const auto generation=session.load();
    if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([id,ticket,reached,generation]{std::scoped_lock lock(mutex);auto it=recoveryPaths.find(id);if(!active||session!=generation||it==recoveryPaths.end()||it->second.ticket!=ticket.c_str())return;it->second.finished=true;it->second.reached=reached;it->second.readyAt=playSeconds;if(!reached){++it->second.failures;it->second.retryAfter=playSeconds+5;}SKSE::log::info("Recovery path finished {:08X}: reached {}",id,reached);});
}





bool shouldWalkRecovery(RE::StaticFunctionTag*,RE::Actor* owner,RE::BSFixedString ticket) {
    if(!owner)return false;std::scoped_lock lock(mutex);auto it=recoveryPaths.find(owner->GetFormID());
    return it!=recoveryPaths.end()&&it->second.ticket==ticket.c_str()&&it->second.generation==session.load()&&it->second.walkRequired;
}
void prepareRecoveryWalk(RE::StaticFunctionTag*,RE::Actor* owner,RE::BSFixedString ticket) {
    if(!owner)return;const auto id=owner->GetFormID();const auto generation=session.load();
    if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([id,ticket,generation]{
        std::scoped_lock lock(mutex);auto it=recoveryPaths.find(id);
        if(!active||session!=generation||it==recoveryPaths.end()||it->second.ticket!=ticket.c_str())return;
        auto* actor=RE::TESForm::LookupByID<RE::Actor>(id);if(!actor)return;
        releaseRecoveryPause(actor);it->second.walkPrepared=true;
        SKSE::ModCallbackEvent event{"PO_WalkEquipped",ticket.c_str(),0,actor};
        if(auto* source=SKSE::GetModCallbackEventSource())source->SendEvent(&event);
        SKSE::log::info("Recovery walking phase ready {:08X}; pause released before path request",id);
    });
}
RE::TESObjectREFR* beginRecoveryWalk(RE::StaticFunctionTag*,RE::Actor* owner,RE::BSFixedString ticket) {
    if(!owner)return nullptr;std::scoped_lock lock(mutex);auto it=recoveryPaths.find(owner->GetFormID());
    if(!active||it==recoveryPaths.end()||it->second.ticket!=ticket.c_str()||it->second.generation!=session.load()||!it->second.walkPrepared||it->second.walking||it->second.finished)return nullptr;
    it->second.walking=true;return it->second.goal.get().get();
}
bool canContinueRecovery(RE::StaticFunctionTag*,RE::Actor* owner,RE::BSFixedString ticket) {
    if(!owner)return false;std::scoped_lock lock(mutex);auto it=recoveryPaths.find(owner->GetFormID());
    if(it==recoveryPaths.end())return false;
    auto goal=it->second.goal.get();auto* cell=owner->GetParentCell();auto* goalCell=goal?goal->GetParentCell():nullptr;
    const bool sameArea=goal&&witnessArea(cell&&cell==goalCell,cell&&goalCell&&!cell->IsInteriorCell()&&!goalCell->IsInteriorCell(),owner->GetWorldspace()&&owner->GetWorldspace()==goal->GetWorldspace());
    return sameArea&&active&&it->second.ticket==ticket.c_str()&&it->second.generation==session.load()&&owner->Is3DLoaded()&&!owner->IsDead()&&!owner->IsInCombat();
}
bool canLookRecovery(RE::StaticFunctionTag*,RE::Actor* owner,RE::BSFixedString ticket) {
    if(!canContinueRecovery(nullptr,owner,ticket))return false;
    auto* process=owner->GetActorRuntimeData().currentProcess;return process&&!process->GetHeadtrackTarget();
}
bool isLookingAtRecoveryGoal(RE::StaticFunctionTag*,RE::Actor* owner,RE::TESObjectREFR* goal) {
    if(!owner||!goal||!goal->IsDynamicForm()||!goal->GetBaseObject()||goal->GetBaseObject()->GetFormID()!=0x3B)return false;
    auto* process=owner->GetActorRuntimeData().currentProcess;return process&&process->GetHeadtrackTarget()==goal->GetHandle();
}
void cleanupRecoveryGoal(RE::StaticFunctionTag*,RE::TESObjectREFR* goal) {
    if(!goal)return;const auto handle=goal->GetHandle();
    if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([handle]{auto marker=handle.get();retireRecoveryGoal(marker.get());});
}


bool getStolenTracking(RE::StaticFunctionTag*) {return stolenTracking.load();}
void setStolenTracking(RE::StaticFunctionTag*,bool enabled) {
    stolenTracking=enabled;
    WritePrivateProfileStringW(L"Settings",L"StolenItemTracking",enabled?L"1":L"0",configPath);
}
bool getStolenTagExpiration(RE::StaticFunctionTag*) {return stolenTagExpiration.load();}
void setStolenTagExpiration(RE::StaticFunctionTag*,bool enabled) {
    stolenTagExpiration=enabled;
    WritePrivateProfileStringW(L"Settings",L"StolenTagExpiration",enabled?L"1":L"0",configPath);
}
bool getEquippedDrops(RE::StaticFunctionTag*) {return equippedDrops.load();}
bool getEquippedDropsReady(RE::StaticFunctionTag*) {return equippedDropsReady.load();}
void setEquippedDrops(RE::StaticFunctionTag*,bool enabled) {
    equippedDrops=enabled;
    WritePrivateProfileStringW(L"Settings",L"EquippedItemDrops",equippedDrops?L"1":L"0",configPath);
}
std::map<RE::FormID,BumpRules> bumpRules;
RE::BGSPerk* capPerk=nullptr;
std::atomic_bool capManaged=false;bool artfulManaged=false;
std::atomic<float> managedMaximum=89;
float resolveMaximum(RE::Actor* player) {float value=perkMaximum(capPerk&&player&&player->HasPerk(capPerk));if(artfulManaged){auto* settings=RE::GameSettingCollection::GetSingleton();auto* setting=settings?settings->GetSetting("fPickPocketMaxChance"):nullptr;value=setting?std::clamp(finite(setting->data.f,97),0.f,100.f):97;}managedMaximum=value;return value;}
bool isCapManaged(RE::StaticFunctionTag*) {return capManaged.load();}
struct KarmaContext {bool active=false,used=false;RE::FormID victim=0;int chance=-1;unsigned maximum=100;};
thread_local KarmaContext karmaContext;
bool getKarmaMode(RE::StaticFunctionTag*) {return karmaMode.load();}
void setKarmaMode(RE::StaticFunctionTag*,bool enabled) {karmaMode=enabled;WritePrivateProfileStringW(L"Settings",L"KarmaMode",enabled?L"1":L"0",configPath);}
std::vector<float> debugValues;std::vector<std::string> debugNames;
bool getForceChance100(RE::StaticFunctionTag*) {return forceChance100.load();}
void setForceChance100(RE::StaticFunctionTag*,bool enabled) {
    std::scoped_lock lock(mutex);forceChance100=enabled;cachedCap=-1;witnessDisplayCache={};debugValues.clear();debugNames.clear();
    WritePrivateProfileStringW(L"Debug",L"Force100Percent",enabled?L"1":L"0",configPath);
}
float* option(int i) {
    switch(i) {case 0:return &config.awareness;case 1:return &config.distraction;case 2:return &config.distance;case 3:return &config.weight;case 4:return &config.value;case 5:return &config.profile;case 6:return &config.witness;case 7:return &config.tagDays;case 8:return &config.recognitionDays;case 9:return &config.maximum;case 10:return &config.pointSkew;default:return nullptr;}
}
constexpr const wchar_t* optionKeys[]={L"AwarenessPoints",L"DistractionPoints",L"DistanceMultiplier",L"WeightPoints",L"ValuePoints",L"ProfilePoints",L"WitnessStrength",L"StolenTagDays",L"RecognitionDays",L"AbsoluteMaximum",L"PointSkew"};
// The INI owns preferences. Co-save records own character history only.
void ensureSettingsFile() {
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(configPath).parent_path(),error);
    if(error){SKSE::log::error("Cannot create INI directory: {}",error.message());return;}
    // CREATE_NEW never truncates an existing user/mod-manager configuration.
    const auto file=CreateFileW(configPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){const auto code=GetLastError();if(code!=ERROR_FILE_EXISTS&&code!=ERROR_ALREADY_EXISTS)SKSE::log::error("Cannot generate settings INI: Windows error {}",code);return;}
    DWORD written=0;
    const bool success=WriteFile(file,initialSettings.data(),static_cast<DWORD>(initialSettings.size()),&written,nullptr)&&written==initialSettings.size();
    CloseHandle(file);
    if(success)SKSE::log::info("Generated missing PickpocketOverhaul.ini with default MCM settings");
    else SKSE::log::error("Could not finish writing generated INI; missing keys will use defaults");
}
void loadReworkSettings();
void loadSettings() {
    ensureSettingsFile();loadReworkSettings();
    config=Config{};
    stolenTracking=GetPrivateProfileIntW(L"Settings",L"StolenItemTracking",1,configPath)!=0;
    stolenTagExpiration=GetPrivateProfileIntW(L"Settings",L"StolenTagExpiration",1,configPath)!=0;
    equippedDrops=GetPrivateProfileIntW(L"Settings",L"EquippedItemDrops",0,configPath)!=0;
    karmaMode=GetPrivateProfileIntW(L"Settings",L"KarmaMode",1,configPath)!=0;
    openingPenalty=GetPrivateProfileIntW(L"Settings",L"InventoryOpeningPenalty",1,configPath)!=0;
    debugMode=GetPrivateProfileIntW(L"Debug",L"Enabled",0,configPath)!=0;
    forceChance100=GetPrivateProfileIntW(L"Debug",L"Force100Percent",0,configPath)!=0;
    for(int i:{2,6,7,8,9,10}) {
        wchar_t text[64];const auto fallback=std::to_wstring(*option(i));
        GetPrivateProfileStringW(L"Settings",optionKeys[i],fallback.c_str(),text,64,configPath);
        wchar_t* end;const float value=std::wcstof(text,&end);
        if(end!=text&&*end==L'\0')*option(i)=value;
    }
    normalize(config);
    SKSE::log::info("Preferences loaded from INI; co-save settings cannot override them");
}
void distanceOverride() {distanceOwned=standing&&standing->SetDistanceOverride(&ownerToken,config.distance);}
float getOption(RE::StaticFunctionTag*,int i) {std::scoped_lock lock(mutex);auto* p=option(i);return i==9&&capManaged.load()?managedMaximum.load():p?*p:0;}
void setOption(RE::StaticFunctionTag*,int i,float value) {
    {std::scoped_lock lock(mutex);auto* p=option(i);if((i==9&&capManaged.load())||!editableOption(i)||!p||!std::isfinite(value))return;*p=value;normalize(config);cachedCap=-1;witnessDisplayCache={};debugValues.clear();debugNames.clear();
    for(int n:{2,6,7,8,9,10})WritePrivateProfileStringW(L"Settings",optionKeys[n],std::to_wstring(*option(n)).c_str(),configPath);}
    if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([]{std::scoped_lock lock(mutex);distanceOverride();});
}
std::vector<float> getDebugValues(RE::StaticFunctionTag*) {std::scoped_lock lock(mutex);return debugValues;}
std::vector<std::string> getDebugNames(RE::StaticFunctionTag*) {std::scoped_lock lock(mutex);return debugNames;}
float currentCap(RE::StaticFunctionTag*) {return cachedCap.load();}
bool ready(RE::StaticFunctionTag*) {return hookReady.load();}
bool getOpeningPenalty(RE::StaticFunctionTag*) {return openingPenalty.load();}
void setOpeningPenalty(RE::StaticFunctionTag*,bool enabled) {
    std::scoped_lock lock(mutex);openingPenalty=enabled;
    if(!enabled)for(auto& [id,state]:states)state.interactionUntil=0;
    cachedCap=-1;witnessDisplayCache={};debugValues.clear();debugNames.clear();
    WritePrivateProfileStringW(L"Settings",L"InventoryOpeningPenalty",enabled?L"1":L"0",configPath);
}
bool getDebugMode(RE::StaticFunctionTag*) {return debugMode.load();}
void setDebugMode(RE::StaticFunctionTag*,bool enabled) {
    debugMode=enabled;WritePrivateProfileStringW(L"Debug",L"Enabled",enabled?L"1":L"0",configPath);
}
bool standingOwned(RE::StaticFunctionTag*) {return distanceOwned.load();}
double days();
#include "ReworkRuntime.h"
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm) {vm->RegisterFunction("GetContrabandProvider","PO_Native",contrabandProvider);vm->RegisterFunction("GetBetterGuard","PO_Native",getBetterGuard);vm->RegisterFunction("SetBetterGuard","PO_Native",setBetterGuard);vm->RegisterFunction("GetCapSource","PO_Native",capSource);vm->RegisterFunction("PrepareRecoveryWalk","PO_Native",prepareRecoveryWalk);vm->RegisterFunction("BeginRecoveryWalk","PO_Native",beginRecoveryWalk);vm->RegisterFunction("IsLookingAtRecoveryGoal","PO_Native",isLookingAtRecoveryGoal);vm->RegisterFunction("CanContinueRecovery","PO_Native",canContinueRecovery);vm->RegisterFunction("CanLookRecovery","PO_Native",canLookRecovery);vm->RegisterFunction("GetStolenTracking","PO_Native",getStolenTracking);vm->RegisterFunction("SetStolenTracking","PO_Native",setStolenTracking);vm->RegisterFunction("GetStolenTagExpiration","PO_Native",getStolenTagExpiration);vm->RegisterFunction("SetStolenTagExpiration","PO_Native",setStolenTagExpiration);vm->RegisterFunction("ShouldWalkRecovery","PO_Native",shouldWalkRecovery);vm->RegisterFunction("GetForceChance100","PO_Native",getForceChance100);vm->RegisterFunction("SetForceChance100","PO_Native",setForceChance100);vm->RegisterFunction("CleanupRecoveryGoal","PO_Native",cleanupRecoveryGoal);vm->RegisterFunction("SetTimerPreview","PO_Native",setTimerPreview);vm->RegisterFunction("BeginRecovery","PO_Native",beginRecovery);vm->RegisterFunction("FinishRecovery","PO_Native",finishRecovery);vm->RegisterFunction("GetEquippedDrops","PO_Native",getEquippedDrops);vm->RegisterFunction("SetEquippedDrops","PO_Native",setEquippedDrops);vm->RegisterFunction("IsEquippedDropsReady","PO_Native",getEquippedDropsReady);vm->RegisterFunction("IsCapManaged","PO_Native",isCapManaged);vm->RegisterFunction("GetKarmaMode","PO_Native",getKarmaMode);vm->RegisterFunction("SetKarmaMode","PO_Native",setKarmaMode);vm->RegisterFunction("GetOpeningPenalty","PO_Native",getOpeningPenalty);vm->RegisterFunction("SetOpeningPenalty","PO_Native",setOpeningPenalty);vm->RegisterFunction("GetDebugMode","PO_Native",getDebugMode);vm->RegisterFunction("SetDebugMode","PO_Native",setDebugMode);vm->RegisterFunction("GetDebugValues","PO_Native",getDebugValues);vm->RegisterFunction("GetDebugNames","PO_Native",getDebugNames);vm->RegisterFunction("GetOption","PO_Native",getOption);vm->RegisterFunction("SetOption","PO_Native",setOption);vm->RegisterFunction("GetCurrentCap","PO_Native",currentCap);vm->RegisterFunction("IsReady","PO_Native",ready);vm->RegisterFunction("IsStandingDistanceOwned","PO_Native",standingOwned);return true;}
double days() {auto* c=RE::Calendar::GetSingleton();return c?c->GetDaysPassed():0;}
void advanceStoredTag(Stolen& item,double now) {
    if(evidenceContainers.contains(item.container)){
        // Confiscated evidence retains vanilla theft ownership. Suspend tag aging
        // while in the evidence chest; recovering it resumes its prior timer.
        if(item.lastTagUpdate>=0&&now>=item.lastTagUpdate)item.expires+=now-item.lastTagUpdate;
        item.lastTagUpdate=now;return;
    }
    auto* ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(item.container);
    const bool stored=ref&&ref->GetBaseObject()&&ref->GetBaseObject()->Is(RE::FormType::Container);
    ageStoredTag(item,now,stored,stolenTracking.load()&&stolenTagExpiration.load());
}

bool bumpAllowed(RE::Actor* actor) {
    auto it=bumpRules.find(actor->GetFormID());
    const BumpRules empty;const auto& rules=it==bumpRules.end()?empty:it->second;
    return rules.allowed(days(),actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSneak),actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kPickpocket));
}
bool following(RE::Actor* a) {return a&&(a->IsPlayerTeammate()||(follower&&a->IsInFaction(follower)));}
bool valid(RE::Actor* a) {return a&&!a->IsPlayerRef()&&!a->IsDead()&&!a->IsDisabled()&&!a->IsDeleted()&&!a->IsSummoned()&&a->GetActorBase()&&a->Get3D();}
std::vector<RE::ActorHandle> loaded() {std::vector<RE::ActorHandle> out;auto* lists=RE::ProcessLists::GetSingleton();if(lists)lists->ForAllActors([&](RE::Actor* a){if(valid(a))out.push_back(a->GetHandle());return RE::BSContainer::ForEachResult::kContinue;});return out;}
bool member(RE::Actor* a,const std::vector<RE::TESFaction*>& groups) {return std::any_of(groups.begin(),groups.end(),[&](auto* f){return a->IsInFaction(f);});}
bool friends(RE::Actor* a,RE::Actor* b) {auto* rel=RE::BGSRelationship::GetRelationship(a->GetActorBase(),b->GetActorBase());return rel&&unsigned(*rel->level)<=unsigned(RE::BGSRelationship::RELATIONSHIP_LEVEL::kFriend);}
bool shared(RE::Actor* a,RE::Actor* b) {return std::any_of(outlawLoyalty.begin(),outlawLoyalty.end(),[&](auto* f){return a->IsInFaction(f)&&b->IsInFaction(f);});}
bool allies(RE::Actor* a,RE::Actor* b) {
    const auto reaction=a->GetFactionReaction(b);
    return friends(a,b)||reaction==RE::FIGHT_REACTION::kAlly||reaction==RE::FIGHT_REACTION::kFriend;
}
bool loyalWitness(RE::Actor* witness,RE::Actor* victim) {
    const bool imperial=member(witness,imperials),stormcloak=member(witness,stormcloaks);
    const bool guild=member(witness,guilds),outlaw=member(witness,outlaws);
    return loyaltyCounts(following(witness),guild,outlaw,imperial||stormcloak,
        (imperial&&member(victim,imperials))||(stormcloak&&member(victim,stormcloaks)),
        friends(witness,victim),guild&&allies(witness,victim),outlaw&&shared(witness,victim));
}
void loadCompatibility(RE::TESDataHandler* data) {
    guilds.clear();outlaws.clear();outlawLoyalty.clear();imperials.clear();stormcloaks.clear();argumentScenes.clear();
    auto add=[&](std::vector<RE::TESFaction*>& group,unsigned id){if(auto* f=data->LookupForm<RE::TESFaction>(id,"Skyrim.esm"))group.push_back(f);};
    for(unsigned id:{0x29DA9u,0x1F259u,0x48362u,0x1BDB3u})add(guilds,id);
    for(unsigned id:{0x1BCC0u,0x60028u,0x29DA9u})add(outlaws,id);
    // Generic BanditFaction classifies outlaws but is NOT a loyalty link between camps.
    for(unsigned id:{0x29DA9u,0xAE026u,0xB8CABu,0x3D729u,0x8E764u,0x88EE4u,0x25F95u}){add(outlawLoyalty,id);add(outlaws,id);}
    for(unsigned id:{0x2BF9Au,0x1C9FCu})add(imperials,id);
    for(unsigned id:{0x2BF9Bu,0x1C9FDu})add(stormcloaks,id);
    // Explicit Skyrim.esm argument scenes, inspected from the installed master.
    for(unsigned id:{0xE4CD7u,0xA3EB7u,0xA3EA6u,0x55FB4u,0x52282u,0x37CC4u,0x2D51Au})
        if(auto* scene=data->LookupForm<RE::BGSScene>(id,"Skyrim.esm"))argumentScenes.insert(scene->GetFormID());
    std::ifstream file("Data/SKSE/Plugins/PickpocketOverhaul_Compatibility.ini");
    std::string line,section;unsigned number=0;
    while(std::getline(file,line)) {
        ++number;auto text=trim(line);if(text.empty()||text.front()==';'||text.front()=='#')continue;
        if(text.front()=='['&&text.back()==']'){section=trim(text.substr(1,text.size()-2));continue;}
        if(auto equal=text.find('=');equal!=text.npos)text=trim(text.substr(equal+1));
        const auto entry=mappedForm(text);
        if(!entry){SKSE::log::warn("Invalid compatibility entry at line {}",number);continue;}
        if(section=="ArgumentScenes") {
            if(auto* scene=data->LookupForm<RE::BGSScene>(entry->localID,entry->plugin))argumentScenes.insert(scene->GetFormID());
            else SKSE::log::warn("Missing/wrong-type argument scene {}|{:X}",entry->plugin,entry->localID);
            continue;
        }
        auto* faction=data->LookupForm<RE::TESFaction>(entry->localID,entry->plugin);
        if(!faction){SKSE::log::warn("Missing/wrong-type mapped faction {}|{:X}",entry->plugin,entry->localID);continue;}
        auto* group=section=="Guild"?&guilds:section=="Outlaw"?&outlaws:section=="OutlawLoyalty"?&outlawLoyalty:section=="Imperial"?&imperials:section=="Stormcloak"?&stormcloaks:nullptr;
        if(!group){SKSE::log::warn("Unknown compatibility section {}",section);continue;}
        if(std::find(group->begin(),group->end(),faction)==group->end())group->push_back(faction);
        if(section=="OutlawLoyalty"&&std::find(outlaws.begin(),outlaws.end(),faction)==outlaws.end())outlaws.push_back(faction);
    }
    SKSE::log::info("Loyalty mappings: guild {}, outlaw {}, local outlaw groups {}, Imperial {}, Stormcloak {}, argument scenes {}",guilds.size(),outlaws.size(),outlawLoyalty.size(),imperials.size(),stormcloaks.size(),argumentScenes.size());
}
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
        auto& trail=attentionTrails[observer->GetFormID()];witnessBump(trail,playSeconds);bumpRules[observer->GetFormID()].witnessed(days());states[observer->GetFormID()].bumpUntil=0;
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
        const bool recognized=observation.recognizedPlayer;
        observation.recognizedPlayer=sees(actor.get(),player)&&std::abs(actor->GetHeadingAngle(player->GetPosition(),false))<70;
        const bool attributed=nativeBump(observation,high->bumpTimer.timeStamp,type,contact,playSeconds);
        const bool accepted=attributed;
        if(changed&&type>=0)SKSE::log::info("Bump observation {:08X}: type {}, distance {:.1f}, player-step {:.1f}, contact {}, recent-contact {}, accepted {}",id,type,distance,step,contact,playSeconds<=observation.contactUntil,accepted);
        if(accepted){
            auto& state=states[id];const auto oldCount=state.bumps;const auto oldUntil=state.bumpUntil;
            auto& trail=attentionTrails[id];
            const float effect=playSeconds-state.lastBump<1?trail.bumpEffect:bumpEffectiveness(trail,playSeconds);
            const bool distinct=playSeconds-state.lastBump>=1;
            bump(state,playSeconds,static_cast<unsigned>(type),effect);
            const bool eligible=bumpAllowed(actor.get());if(!eligible)state.bumpUntil=0;
            auto& rule=bumpRules[id];
            rule.contact(days(),state.bumpUntil,distinct,state.bumpUntil>playSeconds,!recognized,eligible);
            if(distinct)SKSE::log::info("Bump policy {:08X}: eligible {}, recognized-before {}, extra-point {}, next-bonus-day {:.5f}, blocked-until-day {:.5f}, observer-memory-until {:.5f}",id,eligible,recognized,rule.bonusUntil>playSeconds,rule.nextBonusDay,rule.blockedUntilDay,rule.recognizedUntilDay);
            if(state.bumps!=oldCount||state.bumpUntil!=oldUntil){
                if(state.bumpUntil>playSeconds){trail.bumpType=static_cast<unsigned>(type);trail.bumpEffect=effect;}
                SKSE::log::info("Native {} bump {:08X}: count {}, distraction {:.1f}, effectiveness {:.0f}%, window {:.2f}s, suspicion {:.1f}",type==1?"large":"small",id,state.bumps,state.bumpUntil>playSeconds?bumpDistraction(trail.bumpType)*trail.bumpEffect:0.f,effect*100,std::max(0.0,state.bumpUntil-playSeconds),state.suspicion);
            }
            if(distinct)observeOtherBump(actors,actor.get(),player);
        }
    }
    std::erase_if(bumpObservations,[&](const auto& entry){return !nearby.contains(entry.first);});
}
Situation reactionFacts(RE::Actor* actor,RE::PlayerCharacter* player) {
    Situation f;
    f.sleeping=actor->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping;
    f.bumpDistracted=!f.sleeping&&bumpAllowed(actor)&&states[actor->GetFormID()].bumpUntil>playSeconds;
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
struct SightProbe {RE::NiPoint3 source,target;Clock::time_point checked;std::uint64_t generation=0;bool visible=false;};
std::map<std::pair<RE::FormID,RE::FormID>,SightProbe> witnessSightCache;
bool witnessSight(RE::Actor* observer,RE::Actor* target) {
    if(witnessSightCache.size()>512)witnessSightCache.clear();
    auto& probe=witnessSightCache[{observer->GetFormID(),target->GetFormID()}];
    const auto now=Clock::now();const auto source=observer->GetPosition(),position=target->GetPosition();
    // Synchronous native eye/head/torso visibility, not an asynchronous detection
    // request which may remain stale while the pickpocket menu pauses AI.
    if(probe.generation!=session.load()||std::chrono::duration<float>(now-probe.checked).count()>=.1f||
       probe.source.GetDistance(source)>.5f||probe.target.GetDistance(position)>.5f) {
        probe.visible=observer->CalculateLOS(target,2.f*std::numbers::pi_v<float>)!=RE::ACTOR_LOS_LOCATION::kNone;
        probe.source=source;probe.target=position;probe.checked=now;probe.generation=session.load();
    }
    return probe.visible;
}
Situation situation(RE::Actor* victim,bool detected,const std::vector<RE::ActorHandle>& actors) {
    auto* player=RE::PlayerCharacter::GetSingleton();auto& s=states[victim->GetFormID()];Situation f;environment(victim,f);
    f.successfulStacks=activeStacks(theftPressure[victim->GetFormID()],playSeconds);
    f.failed=s.failedUntil>playSeconds;
    f.attention=25+s.suspicion+(s.failedUntil>playSeconds?25:0)+(s.observedUntil>playSeconds?12:0)+(openingPenalty.load()&&s.interactionUntil>playSeconds?8:0);
    f.facing=std::abs(victim->GetHeadingAngle(player->GetPosition(),false))<70;
    f.close=victim->GetDistance(player)<65;f.detected=detected;
    f.sleeping=victim->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping;f.sitting=victim->AsActorState()->IsSitting();f.talking=victim->GetCurrentScene()!=nullptr;
    auto* ui=RE::UI::GetSingleton();
    f.speakingToPlayer=ui&&!ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)&&nativeCondition(victim,RE::FUNCTION_DATA::FunctionID::kIsTalking)&&nativeCondition(victim,RE::FUNCTION_DATA::FunctionID::kIsGreetingPlayer);
    if(auto* proc=victim->GetActorRuntimeData().currentProcess) {auto focus=proc->GetHeadtrackTarget().get();f.focusedElsewhere=focus&&focus.get()!=player;}
    f.bumpDistracted=!f.sleeping&&bumpAllowed(victim)&&s.bumpUntil>playSeconds;
    auto rule=bumpRules.find(victim->GetFormID());f.bumpBonus=f.bumpDistracted&&rule!=bumpRules.end()&&rule->second.bonusUntil>playSeconds;
    f.bumpEffect=attentionTrails[victim->GetFormID()].bumpEffect;
    f.fleeing=fleeingActors.contains(victim->GetFormID());
    Commotion combined;
    if(auto it=commotions.find(victim->GetFormID());it!=commotions.end())combined=it->second;
    if(s.commotionUntil>playSeconds)combined.add(Noise::Combat,10,playSeconds,1);
    if(f.bumpDistracted)combined.add(Noise::Meal,bumpDistraction(attentionTrails[victim->GetFormID()].bumpType)*f.bumpEffect,playSeconds,1);
    f.misdirection=combined.value(playSeconds);
    unsigned eligible=0,rear=0,occluded=0,counted=0;
    std::set<RE::FormID> considered;std::string witnessIDs;
    for(auto& handle:actors) if(auto other=handle.get();valid(other.get())&&other.get()!=victim&&other.get()!=player&&other->GetDistance(victim)<900) {
        if(!considered.insert(other->GetFormID()).second)continue;
        auto* cell=other->GetParentCell();auto* victimCell=victim->GetParentCell();
        const bool sameCell=cell&&cell==victimCell;
        const bool exterior=cell&&victimCell&&!cell->IsInteriorCell()&&!victimCell->IsInteriorCell();
        const bool sameWorld=other->GetWorldspace()&&other->GetWorldspace()==victim->GetWorldspace();
        if(!witnessArea(sameCell,exterior,sameWorld))continue;
        if(following(other.get())||other->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping)continue;
        if(other->GetDistance(victim)<250)++f.crowd;
        if(!loyalWitness(other.get(),victim))continue;
        ++eligible;
        float focusAngle=0;
        if(auto* process=other->GetActorRuntimeData().currentProcess)if(auto focus=process->GetHeadtrackTarget().get();focus)focusAngle=other->GetHeadingAngle(focus->GetPosition(),false);
        const auto playerAngle=relativeViewAngle(other->GetHeadingAngle(player->GetPosition(),false),focusAngle);
        const auto victimAngle=relativeViewAngle(other->GetHeadingAngle(victim->GetPosition(),false),focusAngle);
        if(!witnessVisible(true,other->GetDistance(player),playerAngle,victimAngle,true,true)){++rear;continue;}
        if(!witnessSight(other.get(),player)||!witnessSight(other.get(),victim)){++occluded;continue;}
        ++counted;witnessIDs+=fmt::format(" {:08X}",other->GetFormID());
        if(other->IsGuard()||other->IsAlarmed())++f.alerted;
        auto it=states.find(other->GetFormID());float suspicious=it==states.end()?0:it->second.suspicion;
        Situation observer;observer.attention=25+suspicious;observer.detected=true;
        observer.detection=std::clamp(float(other->RequestDetectionLevel(player))/100.f,0.f,1.f);
        observer.facing=std::abs(playerAngle)<70;
        observer.fleeing=fleeingActors.contains(other->GetFormID());observer.close=other->GetDistance(player)<65;observer.sitting=other->AsActorState()->IsSitting();observer.talking=other->GetCurrentScene()!=nullptr;
        if(auto* process=other->GetActorRuntimeData().currentProcess){auto focus=process->GetHeadtrackTarget().get();observer.focusedElsewhere=focus&&focus.get()!=player;}
        environment(other.get(),observer,false);
        f.witnessPenalty+=std::clamp(attention(observer)*.12f+(other->IsGuard()?4:0),0.f,12.f);
    }
    f.witnessPenalty=std::min(f.witnessPenalty,45.f);
    static std::string lastScan;
    const auto scan=fmt::format("session {}, target {:08X}: eligible {}, outside view/range {}, occluded {}, witnesses {}, raw {:.1f}, IDs{}",session.load(),victim->GetFormID(),eligible,rear,occluded,counted,f.witnessPenalty,witnessIDs);
    if(scan!=lastScan){SKSE::log::info("Witness scan (direct native LOS): {}",scan);lastScan=scan;}
    return f;
}
std::int32_t chance(float thiefSkill,float targetSkill,std::uint32_t value,float weight,RE::Actor* thief,RE::Actor* target,bool detected,RE::TESForm* item,bool preview=false) {
    if(!active||!thief||!thief->IsPlayerRef()||!valid(target))return original(thiefSkill,targetSkill,value,weight,thief,target,detected,item);
    std::scoped_lock lock(mutex);
    if(forceChance100.load()) {
        cachedCap=100;
        if(karmaContext.active)karmaContext.maximum=100;
        if(preview){witnessDisplayCache={target->GetHandle().native_handle(),100,0,true};debugValues.assign(20,0);debugValues[0]=debugValues[1]=debugValues[3]=debugValues[4]=100;debugValues[2]=thiefSkill;debugValues[19]=100;debugNames={target->GetName(),item?item->GetName():""};}
        return 100;
    }
    auto* ini=RE::INISettingCollection::GetSingleton();auto* setting=ini?ini->GetSetting("fActivatePickLength:Interface"):nullptr;
    const float reach=setting&&std::isfinite(setting->data.f)&&setting->data.f>0?setting->data.f:180.f;
    if(thief->GetDistance(target)>reach*config.distance)return 0;
    Config balance=config;if(capManaged.load())balance.maximum=resolveMaximum(thief);
    auto actors=loaded();auto f=situation(target,detected,actors);f.weight=weight;f.value=value;
    f.detection=std::clamp(float(target->RequestDetectionLevel(thief))/100.f,0.f,1.f);
    f.profileRisk=target->IsGuard()?1.f:member(target,outlaws)?.4f:0.f;
    const auto race=target->GetRace();auto* base=target->GetActorBase();auto* baseRace=base->GetRace();auto* voice=base->GetVoiceType();
    if(!voice)if(auto* face=base->GetRootFaceNPC())voice=face->GetVoiceType();
    const bool elderly=elderRecord(race?race->GetFormID():0,baseRace?baseRace->GetFormID():0,voice?voice->GetFormID():0);
    f.ageRelief=elderly?.75f:0.f;
    f.lingeringReaction=sampleReaction(attentionTrails[target->GetFormID()],f,playSeconds);
    if(!preview&&karmaContext.active)karmaContext.maximum=capManaged.load()?static_cast<unsigned>(balance.maximum+(f.bumpDistracted&&f.bumpBonus&&!f.sleeping?1:0)):100;
    if(artfulManaged){f.bumpBonus=false;if(!preview&&karmaContext.active)karmaContext.maximum=static_cast<unsigned>(balance.maximum);}
    cachedCap=cap(f,balance,thiefSkill);float hand=handChance(weight,value,balance,thiefSkill);
    // Same argument order as vanilla ComputePickpocketSuccess. Apply perk/item
    // entry points before independent attention/witness penalties and final cap.
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPickpocketChance,thief,target,item,&hand);
    const auto result=finalChance(hand,thiefSkill,f,balance);
    if(preview){
        witnessDisplayCache={target->GetHandle().native_handle(),result,combinedAwarenessCapEffect(f,balance,thiefSkill),true};
        debugValues={float(result),cap(f,balance,thiefSkill),thiefSkill,skillStartingCeiling(config,thiefSkill),hand,
            awarenessLoss(f,balance),witnessCapEffect(f,balance,thiefSkill),balance.pointSkew*balance.weight*weightBurden(weight),balance.pointSkew*balance.value*valueBurden(value),
            -balance.profile*std::clamp(f.profileRisk-f.ageRelief,-1.f,1.f),balance.distraction*distraction(f),skillCapRelief(f,balance,thiefSkill),
            sleepRelief(f,balance),f.bumpDistracted?f.bumpEffect*100:0.f,float(std::max(0.0,states[target->GetFormID()].bumpUntil-playSeconds)),
            f.failed?1.f:0.f,witnessLoss(f,balance),ageBenefit(f,balance),float(f.successfulStacks),stackChanceRetention(f.successfulStacks,thiefSkill)*100};
        const char* actorName=target->GetName();const char* itemName=item?item->GetName():nullptr;
        debugNames={actorName&&*actorName?actorName:"Unnamed NPC",itemName&&*itemName?itemName:"Unnamed item"};
    }
    // Bounded diagnostic: one record at most per second when selection/state/result
    // changes. No actor pointers or growing per-item diagnostic cache are retained.
    struct Trace {RE::FormID target=0,item=0;bool sleeping=false,bump=false;int result=-1;float witness=-1,profile=-1;Clock::time_point time{};};
    static Trace trace;
    const auto now=Clock::now();const auto itemID=item?item->GetFormID():0;
    if((trace.target!=target->GetFormID()||trace.item!=itemID||trace.sleeping!=f.sleeping||trace.bump!=f.bumpDistracted||trace.result!=result||trace.witness!=balance.witness||trace.profile!=balance.profile)&&now-trace.time>=std::chrono::seconds(1)){
        SKSE::log::info("Chance target {:08X}, item {:08X}: actor-state-offset {:X}, sleep-state {}, sleeping {}, skill {:.1f}, weight {:.2f}, value {}, witnesses {:.1f}, awareness-loss {:.1f}, sleep-relief {:.1f}, bump-active {}, bump-left {:.2f}, bump-count {}, misdirection {:.1f}, facing {}, detected {}, suspicion {:.1f}, witness-strength {:.1f}, witness-loss {:.1f}, witness-cap-effect {:.2f}, race {:08X}, voice {:08X}, elderly {}, age-relief {:.2f}, occupation-risk {:.2f}, hour {:.2f}, time-attention {:.2f}, guard {}, weather-noise {:.2f}, skill-starting-ceiling {:.2f}, skill-cap-relief {:.2f}, profile-points {:.2f}, lingering-reaction {:.2f}, bump-effectiveness {:.2f}, failed {}, stolen-stacks {}, stack-retention {:.2f}, hand {:.1f}, cap {:.1f}, chance {}",target->GetFormID(),itemID,reinterpret_cast<std::uintptr_t>(target->AsActorState())-reinterpret_cast<std::uintptr_t>(target),static_cast<unsigned>(target->AsActorState()->GetSitSleepState()),f.sleeping,thiefSkill,weight,value,f.witnessPenalty,awarenessLoss(f,balance),sleepRelief(f,balance),f.bumpDistracted,std::max(0.0,states[target->GetFormID()].bumpUntil-playSeconds),states[target->GetFormID()].bumps,f.misdirection,f.facing,f.detected,states[target->GetFormID()].suspicion,balance.witness,witnessLoss(f,balance),witnessCapEffect(f,balance,thiefSkill),race?race->GetFormID():0,voice?voice->GetFormID():0,elderly,ageBenefit(f,balance),f.profileRisk,f.hour,timeAwareness(f.hour,f.guard),f.guard,weatherDistraction(f),skillStartingCeiling(config,thiefSkill),skillCapRelief(f,balance,thiefSkill),balance.profile,f.lingeringReaction,f.bumpEffect,f.failed,f.successfulStacks,stackChanceRetention(f.successfulStacks,thiefSkill),hand,cachedCap.load(),result);
        trace={target->GetFormID(),itemID,f.sleeping,f.bumpDistracted,result,balance.witness,balance.profile,now};
    }
    return result;
}
template<unsigned Index>
std::int32_t chanceSite(float skill,float targetSkill,std::uint32_t value,float weight,RE::Actor* thief,RE::Actor* target,bool detected,RE::TESForm* item) {
    if(!active||!thief||!thief->IsPlayerRef()||!valid(target))return previousChance[Index](skill,targetSkill,value,weight,thief,target,detected,item);
    if constexpr(Index==1){std::scoped_lock lock(mutex);witnessDisplayCache.valid=false;}
    const auto result=chance(skill,targetSkill,value,weight,thief,target,detected,item,Index==1);
    if constexpr(Index==0)if(karmaContext.active&&karmaContext.victim==target->GetFormID())karmaContext.chance=result;
    return result;
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
        SKSE::AllocTrampoline(1024);
        auto& trampoline=SKSE::GetTrampoline();
        trampoline.write_call<5>(base+sites[0].rva,chanceSite<0>);
        trampoline.write_call<5>(base+sites[1].rva,chanceSite<1>);
        SKSE::log::info("Installed both shared chance hooks (menu and theft roll); player chances owned by Pickpocket Chance Rework");
        return true;
    }catch(const std::exception& e){SKSE::log::error("Chance hook validation failed: {}; overhaul disabled",e.what());return false;}
}
using AdvanceFn=void(*)(RE::ContainerMenu*,float,std::uint32_t);
AdvanceFn previousAdvance=nullptr;
void advanceContainer(RE::ContainerMenu* menu,float interval,std::uint32_t currentTime) {
    previousAdvance(menu,interval,currentTime);
    if(!menu||!menu->uiMovie)return;
    auto* movie=menu->uiMovie.get();
    // The text is edited only on the menu's own Scaleform update thread, after
    // the normal update. No actor scans or chance recalculation happen here.
    struct Decoration {RE::GFxMovieView* movie=nullptr;std::string base,decorated;};
    static Decoration textState;
    if(textState.movie!=movie){textState={movie,{},{}};}
    RE::GFxValue field,html;
    constexpr auto fieldPath="_root.Menu_mc.itemCardFadeHolder.StealTextInstance.PercentTextInstance";
    if(!movie->GetVariable(&field,fieldPath)||!(field.IsObject()||field.IsDisplayObject())||!field.GetMember("htmlText",&html)||!html.IsString())return;
    const std::string current=html.GetString();
    if(current!=textState.decorated){textState.base=current;textState.decorated.clear();}
    WitnessDisplay display;{std::scoped_lock lock(mutex);display=witnessDisplayCache;}
    RE::GFxValue segment,cardChance;
    const bool applicable=debugMode.load()&&active&&display.valid&&RE::ContainerMenu::GetContainerMode()==RE::ContainerMenu::ContainerMode::kPickpocket&&
        RE::ContainerMenu::GetTargetRefHandle()==display.targetHandle&&
        movie->GetVariable(&segment,"_root.Menu_mc.inventoryLists.categoryList.activeSegment")&&segment.IsNumber()&&segment.GetNumber()==0&&
        movie->GetVariable(&cardChance,"_root.Menu_mc.itemCard.itemInfo.pickpocketChance")&&cardChance.IsNumber()&&cardChance.GetNumber()==display.chance;
    const auto desired=applicable?witnessDisplay(textState.base,display.chance,display.effect):textState.base;
    if(desired==current)return;
    if(field.SetTextHTML(desired.c_str())){
        RE::GFxValue updated;
        textState.decorated=applicable?desired:std::string{};
        // Scaleform can canonicalize HTML; cache its returned representation
        // so subsequent frames do not append the suffix again.
        if(applicable&&field.GetMember("htmlText",&updated)&&updated.IsString())textState.decorated=updated.GetString();
        if(applicable){static bool logged=false;if(!logged){SKSE::log::info("Witness percentage annotation applied to the native chance text");logged=true;}}
    }
}
bool installWitnessDisplay() {
    try {
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_ContainerMenu[0]};
        const auto delegate=*reinterpret_cast<std::uintptr_t*>(table.address()+5*sizeof(void*));
        if(!executableAddress(delegate))throw std::runtime_error("Container menu update destination not executable");
        previousAdvance=reinterpret_cast<AdvanceFn>(table.write_vfunc(5,advanceContainer));
        SKSE::log::info("Witness display installed: unsigned rounded percentage-point effect on the situational cap");return true;
    }catch(const std::exception& e){SKSE::log::error("Witness display unavailable: {}",e.what());return false;}
}
using JournalAdvanceFn=void(*)(RE::JournalMenu*,float,std::uint32_t);
JournalAdvanceFn previousJournalAdvance=nullptr;
void advanceJournal(RE::JournalMenu* menu,float interval,std::uint32_t time) {
    previousJournalAdvance(menu,interval,time);
    if(!menu||!menu->uiMovie)return;
    auto* movie=menu->uiMovie.get();RE::GFxValue field,panel;
    const auto index=timerPreview.load();
    const bool show=(index==1||index==3)&&movie->GetVariable(&panel,"_root.ConfigPanelFader.configPanel")&&panel.IsDisplayObject();
    if(!movie->GetVariable(&field,"_root.POTimerPreview")||!field.IsDisplayObject()) {
        if(!show)return;
        const auto rect=movie->GetVisibleFrameRect();const auto width=rect.right-rect.left,height=rect.bottom-rect.top;
        RE::GFxValue args[6]={RE::GFxValue("POTimerPreview"),RE::GFxValue(15900.0),RE::GFxValue(double(rect.left+width*.72f)),RE::GFxValue(double(rect.top+height*.19f)),RE::GFxValue(double(width*.27f)),RE::GFxValue(double(height*.65f))};
        movie->Invoke("_root.createTextField",nullptr,args,6);
        if(!movie->GetVariable(&field,"_root.POTimerPreview")||!field.IsDisplayObject()){static bool reported=false;if(!reported){SKSE::log::error("MCM timer overlay: text field creation failed");reported=true;}return;}
        field.SetMember("multiline",RE::GFxValue(true));field.SetMember("wordWrap",RE::GFxValue(true));field.SetMember("selectable",RE::GFxValue(false));
        SKSE::log::info("MCM right-side timer text field created");
    }
    field.SetMember("_visible",RE::GFxValue(show));if(!show)return;
    float scale;{std::scoped_lock lock(mutex);scale=index==1?config.tagDays:config.recognitionDays;}
    auto text=fmt::format("<font face='$EverywhereFont' size='18' color='#FFFFFF'><b>{}</b><br><br>Days = base x factor<br>Factor = 1 + ln(value + 1) / 4<br><br>",index==1?"Stolen tag duration":"Recognition duration");
    for(unsigned value:{10u,50u,100u,1000u,5000u})text+=fmt::format("{} gold: {:.1f} days<br>",value,memoryDays(value,scale));
    text+="<br>Higher values take longer.</font>";
    static RE::GFxMovieView* lastMovie=nullptr;static std::string lastText;
    if(lastMovie!=movie){lastMovie=movie;lastText.clear();}
    if(lastText!=text&&field.SetTextHTML(text.c_str()))lastText=std::move(text);
}
void installTimerPreview() {
    try{REL::Relocation<std::uintptr_t> table{RE::VTABLE_JournalMenu[0]};const auto target=*reinterpret_cast<std::uintptr_t*>(table.address()+5*sizeof(void*));if(!executableAddress(target))throw std::runtime_error("Journal update target invalid");previousJournalAdvance=reinterpret_cast<JournalAdvanceFn>(table.write_vfunc(5,advanceJournal));SKSE::log::info("MCM timer overlay installed on journal movie thread");}catch(const std::exception& e){SKSE::log::error("MCM timer overlay unavailable: {}",e.what());}
}
void accountPendingTransfer() {
    if(!pendingTransfer.consume())return;
    const auto duration=std::uniform_real_distribution<double>(60.0,120.0)(pressureRandom);
    auto& pressure=theftPressure[pendingTransfer.victim];successfulStack(pressure,playSeconds,duration);
    cachedCap=-1;witnessDisplayCache={};debugValues.clear();debugNames.clear();
    SKSE::log::info("Successful stolen stack {:08X}, item {:08X}: count {}, penalty window {:.2f}s",pendingTransfer.victim,pendingTransfer.item,pressure.stacks,duration);
}
using PercentileFn=std::uint32_t(*)(void*,std::uint32_t);
PercentileFn previousPercentile=nullptr;
std::uint32_t percentile(void* generator,std::uint32_t bound) {
    const auto ordinary=previousPercentile(generator,bound);
    if(!karmaContext.active||karmaContext.used||karmaContext.chance<0||bound!=100)return ordinary;
    karmaContext.used=true;
    const auto chance=std::clamp(karmaContext.chance,0,100);
    std::scoped_lock lock(mutex);
    const auto effective=std::min(karma.effective(chance),karmaContext.maximum);
    const auto result=chance>0&&ordinary>=static_cast<unsigned>(chance)&&ordinary<effective?static_cast<unsigned>(chance-1):ordinary;
    SKSE::log::info("Karma bad-luck roll: base {}, effective {}, percentile {}, credit {}",chance,effective,ordinary,karma.credit);
    return result;
}
void installKarma(std::span<const std::uint8_t> file,std::uint32_t attemptRva) {
    try {
        const auto site=Hooks::karmaSite(file,attemptRva);const auto base=REL::Module::get().base();const auto address=base+site.rva;
        if(!executableAddress(address)||!executableAddress(address+12))throw std::runtime_error("RNG site outside executable memory");
        const std::span<const std::uint8_t> live(reinterpret_cast<const std::uint8_t*>(address-8),21);
        if(!Hooks::contextMatches(file,site,live))throw std::runtime_error("RNG call context changed");
        const auto delegate=static_cast<std::uintptr_t>(Hooks::destination(live,8,address));
        if(!executableAddress(delegate))throw std::runtime_error("RNG delegate not executable");
        previousPercentile=reinterpret_cast<PercentileFn>(delegate);SKSE::GetTrampoline().write_call<5>(address,percentile);karmaReady=true;
        SKSE::log::info("Installed isolated karma percentile hook at RVA {:X}",site.rva);
    }catch(const std::exception& e){SKSE::log::error("Karma RNG hook unavailable: {}; vanilla RNG retained",e.what());}
}
struct ScopedKarma {
    KarmaContext previous;
    ScopedKarma(bool enabled,RE::FormID victim):previous(karmaContext){karmaContext={enabled,false,victim,-1};}
    ~ScopedKarma(){karmaContext=previous;}
};
// A request lives only on the menu thread and only until the next taking transfer.
// No player inventory detour, generic Actor vtable replacement or raw saved pointers.
RE::TESForm* dropOwner(RE::Actor* actor) {
    auto* base=actor->GetActorBase();
    if(base&&base->IsUnique()&&!base->IsDynamicForm())return base;
    if(auto* faction=actor->GetCrimeFaction();faction&&!faction->IsDynamicForm())return faction;
    if(auto* templateBase=actor->GetTemplateBase();templateBase&&!templateBase->IsDynamicForm())return templateBase;
    if(base&&!base->IsDynamicForm())return base;
    for(auto* faction:outlaws)if(!faction->IsDynamicForm()&&actor->IsInFaction(faction))return faction;
    return nullptr;
}
void assignDropOwner(RE::TESObjectREFR* ground,RE::TESForm* owner) {
    ground->SetOwner(owner);ground->extraList.SetOwner(owner);
    ground->AddChange(RE::TESObjectREFR::ChangeFlags::kOwnershipExtra);
}
RE::ObjectRefHandle dropOriginal(RE::Actor* actor,RE::TESBoundObject* object,RE::ExtraDataList* extra,RE::TESForm* owner,std::uint32_t uidBase,std::uint16_t unique) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    const bool playerOwned=player&&extra&&(extra->GetOwner()==player||extra->GetOwner()==player->GetActorBase());
    if(playerOwned)owner=player->GetActorBase();
    auto dropped=actor->DropObject(object,extra,1,nullptr,nullptr);
    auto ground=dropped.get();
    if(!ground){SKSE::log::error("Delayed/original drop {:08X}: native drop returned no reference",object->GetFormID());return dropped;}
    // Read back the resulting instance identity: the engine can rebase its ID
    // while moving an extra list onto a world reference.
    auto* groundUID=ground->extraList.GetByType<RE::ExtraUniqueID>();
    if(!groundUID){ground->extraList.Add(new RE::ExtraUniqueID(uidBase,unique));groundUID=ground->extraList.GetByType<RE::ExtraUniqueID>();}
    if(groundUID){uidBase=groundUID->baseID;unique=groundUID->uniqueID;}
    ground->AddChange(RE::TESObjectREFR::ChangeFlags::kItemExtraData);
    reportedGroundPickups.erase(ground->GetFormID());
    assignDropOwner(ground.get(),owner);
    equipRetries.erase(ground->GetFormID());recoverySounds.erase(ground->GetFormID());recoveryProgress.erase(ground->GetFormID());
    dropRecoveries.push_back({actor->GetFormID(),ground->GetFormID(),object->GetFormID(),uidBase,unique});
    SKSE::log::info("Equipped ground owner: reference {:08X}, expected {:08X}, actual {:08X}",ground->GetFormID(),owner->GetFormID(),ground->GetOwner()?ground->GetOwner()->GetFormID():0);
    Stolen tracked;tracked.victim=actor->GetFormID();tracked.form=object->GetFormID();tracked.owner=owner->GetFormID();tracked.container=ground->GetFormID();tracked.uidBase=uidBase;tracked.uid=unique;
    tracked.value=static_cast<unsigned>(std::max(0,object->GetGoldValue()));const auto now=days();tracked.lastTagUpdate=now;tracked.originHold=currentHold(actor)?currentHold(actor)->GetFormID():0;tracked.stolenDay=now;tracked.noticed=now+(20+std::min(tracked.value,1000u)*.03)/86400;tracked.expires=now+memoryDays(tracked.value,config.tagDays);tracked.recognizesUntil=now+memoryDays(tracked.value,config.recognitionDays);if(stolenTracking.load()&&!playerOwned)stolen.push_back(tracked);
    return dropped;
}
struct EquippedRequest {
    RE::FormID victim=0,form=0;RE::ExtraDataList* instance=nullptr;
    std::uint64_t generation=0;bool success=false;
};
thread_local EquippedRequest equippedRequest;
using MemberTransferFn=RE::ObjectRefHandle*(*)(RE::TESObjectREFR*,RE::ObjectRefHandle*,RE::TESBoundObject*,std::int32_t,RE::ITEM_REMOVE_REASON,RE::ExtraDataList*,RE::TESObjectREFR*,const RE::NiPoint3*,const RE::NiPoint3*);
RE::ObjectRefHandle* transferEquipped(RE::TESObjectREFR* source,RE::ObjectRefHandle* result,RE::TESBoundObject* object,std::int32_t count,RE::ITEM_REMOVE_REASON reason,RE::ExtraDataList* extra,RE::TESObjectREFR* destination,const RE::NiPoint3* location,const RE::NiPoint3* rotation) {
    const auto request=equippedRequest;equippedRequest={};
    // Preserve dynamic dispatch and any existing source RemoveItem replacement.
    const auto delegate=reinterpret_cast<MemberTransferFn>((*reinterpret_cast<std::uintptr_t**>(source))[0x56]);
    auto forward=[&]{return delegate(source,result,object,count,reason,extra,destination,location,rotation);};
    auto* actor=source?source->As<RE::Actor>():nullptr;
    std::scoped_lock lock(mutex);
    const bool sleeping=actor&&actor->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSleeping;
    const bool worn=extra&&(extra->HasType<RE::ExtraWorn>()||extra->HasType<RE::ExtraWornLeft>());
    const bool supported=object&&(object->Is(RE::FormType::Armor)||object->Is(RE::FormType::Weapon));
    if(!active||request.generation!=session.load()||!valid(actor)||!destination||!destination->IsPlayerRef()||
       !object||!matchesEquippedTransfer(request.victim,request.form,reinterpret_cast<std::uintptr_t>(request.instance),request.generation,actor->GetFormID(),object->GetFormID(),reinterpret_cast<std::uintptr_t>(extra),session.load())||dropRecoveries.size()+deferredDrops.size()>=10000||
       !shouldDropEquipped(equippedDrops.load(),equippedDropsReady.load(),request.success,true,sleeping,worn,supported,count))return forward();
    auto* owner=dropOwner(actor);if(!owner){SKSE::log::warn("Equipped drop {:08X}: no static owner; normal transfer retained",actor->GetFormID());return forward();}
    auto* changes=actor->GetInventoryChanges();if(!changes)return forward();
    auto* uid=extra->GetByType<RE::ExtraUniqueID>();
    if(!uid){extra->Add(new RE::ExtraUniqueID(actor->GetFormID(),changes->GetNextUniqueID()));uid=extra->GetByType<RE::ExtraUniqueID>();}
    if(!uid)return forward();
    auto uidBase=uid->baseID;auto unique=uid->uniqueID;
    actor->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
    // Native member ABI has an explicit hidden handle result after 'this'.
    // The original RemoveItem implementation retains tempering/enchantment data.
    // Let the engine choose its normal drop origin, orientation and physics
    // setup. Forcing the model origin eight units above a slope can embed its
    // collision shape. Preserve the original instance; never spawn a copy.
    auto dropped=dropOriginal(actor,object,extra,owner,uidBase,unique);
    std::construct_at(result,std::move(dropped));
    // A successful redirected stack is consumed once, without synthesizing a
    // player transfer event or a second theft roll/crime notification.
    if(pendingTransfer.victim==request.victim&&pendingTransfer.item==request.form&&pendingTransfer.success&&!pendingTransfer.counted){pendingTransfer.transferred=true;accountPendingTransfer();}
    SKSE::log::info("Equipped transfer redirected immediately: NPC {:08X}, item {:08X}",request.victim,request.form);
    return result;
}
bool installEquippedTransfers(std::span<const std::uint8_t> file,const Hooks::Site& attemptSite) {
    try {
        const auto site=Hooks::equippedTransferSite(file,attemptSite);const auto address=REL::Module::get().base()+site.rva;
        if(!executableAddress(address-8)||!executableAddress(address+13)||site.fileOffset<8||file.size()-site.fileOffset<14||
           std::memcmp(file.data()+site.fileOffset-8,reinterpret_cast<const void*>(address-8),22)!=0)throw std::runtime_error("Equipped-transfer live context changed");
        SKSE::GetTrampoline().write_call<6>(address,transferEquipped);equippedDropsReady=true;
        SKSE::log::info("Installed scoped equipped transfer hook at RVA {:X}; default disabled",site.rva);return true;
    }catch(const std::exception& e){equippedDropsReady=false;SKSE::log::error("Equipped drops unavailable: {}; normal transfers retained",e.what());return false;}
}
using AttemptFn=bool(*)(RE::PlayerCharacter*,RE::TESObjectREFR*,RE::InventoryEntryData*,std::int32_t,bool);
AttemptFn previousAttempt=nullptr;
bool attempt(RE::PlayerCharacter* player,RE::TESObjectREFR* container,RE::InventoryEntryData* entry,std::int32_t number,bool fromContainer) {
    equippedRequest={};
    const auto generation=session.load();
    auto* actor=container?container->As<RE::Actor>():nullptr;
    const bool tracked=active&&player&&player->IsPlayerRef()&&valid(actor)&&entry&&number>0;
    const auto id=tracked?actor->GetFormID():0;
    std::uint64_t ticket=0;
    if(tracked&&fromContainer&&entry->object){std::scoped_lock lock(mutex);ticket=++nextTicket;pendingTicket=ticket;pendingTransfer={id,entry->object->GetFormID(),playSeconds+2};pendingTransfer.amount=static_cast<unsigned>(number);pendingTransfer.unitValue=static_cast<unsigned>(std::max(0,entry->GetValue()));pendingTransfer.unitWeight=std::max(0.f,entry->GetWeight());pendingTransfer.equipped=entry->IsWorn();auto* owner=entry->GetOwner();pendingTransfer.playerOwned=owner&&(owner==player->GetActorBase()||owner==player);}
    EquippedRequest dropCandidate;
    if(tracked&&fromContainer&&equippedDrops.load()&&equippedDropsReady.load()&&number==1&&entry->object&&!entry->IsQuestObject()&&entry->extraLists) {
        for(auto* x:*entry->extraLists)if(x&&(x->HasType<RE::ExtraWorn>()||x->HasType<RE::ExtraWornLeft>())) {
            if(dropCandidate.instance){dropCandidate={};break;}
            dropCandidate={id,entry->object->GetFormID(),x,generation,false};
        }
    }
    ScopedKarma karmaScope(tracked&&fromContainer&&karmaMode.load()&&karmaReady.load(),id);
    const bool success=previousAttempt(player,container,entry,number,fromContainer);
    if(tracked&&active&&session==generation){
        std::scoped_lock lock(mutex);
        if(karmaContext.active&&karmaContext.used)karma.outcome(static_cast<unsigned>(karmaContext.chance),success);
        if(ticket&&pendingTicket==ticket){if(success){pendingTransfer.success=true;accountPendingTransfer();}else pendingTransfer={};}
        if(!success){if(fromContainer)bumpRules[id].failed(days());failedTheft(states[id],playSeconds);SKSE::log::info("Failed theft {:08X}: 80 percent chance reduction for 60 seconds",id);}
    }
    if(success&&dropCandidate.instance&&active&&session==generation){dropCandidate.success=true;equippedRequest=dropCandidate;}
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
        installKarma(file,static_cast<std::uint32_t>(target.address()-base));
        previousAttempt=reinterpret_cast<AttemptFn>(delegate);
        SKSE::GetTrampoline().write_call<5>(address,attempt);
        installEquippedTransfers(file,sites[0]);
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
    if(!container)return nullptr;
    if(container->GetBaseObject()&&container->GetBaseObject()->GetFormID()==s.form){auto* uid=container->extraList.GetByType<RE::ExtraUniqueID>();return uid&&uid->baseID==s.uidBase&&uid->uniqueID==s.uid?&container->extraList:nullptr;}
    auto inv=container->GetInventory([&](RE::TESBoundObject& o){return o.GetFormID()==s.form;});
    for(auto& [object,e]:inv)if(e.first>0&&e.second&&e.second->extraLists)for(auto* x:*e.second->extraLists)if(x)if(auto* id=x->GetByType<RE::ExtraUniqueID>();id&&id->baseID==s.uidBase&&id->uniqueID==s.uid)return x;
    return nullptr;
}
void processDeferredDrops() {
    auto* ui=RE::UI::GetSingleton();if(!ui||ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME))return;
    for(auto& drop:deferredDrops)drop.after=playSeconds; // migrate old queued originals without adding further delay
    std::erase_if(deferredDrops,[&](const DeferredDrop& drop){
        if(playSeconds<drop.after)return false;
        auto* actor=RE::TESForm::LookupByID<RE::Actor>(drop.victim);
        if(!actor||actor->IsDead()||actor->IsDeleted())return true;
        if(!valid(actor))return false;
        Stolen key;key.form=drop.form;key.uidBase=drop.uidBase;key.uid=drop.uid;
        auto* extra=instance(actor,key);auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(drop.form);auto* owner=dropOwner(actor);
        if(!extra||!object||!owner){SKSE::log::info("Unfastened item {:08X}: original unavailable; no replacement spawned",drop.form);return true;}
        dropOriginal(actor,object,extra,owner,drop.uidBase,drop.uid);return true;
    });
}
bool clearNoiseRay(RE::Actor* actor,RE::NiPoint3 target) {
    auto* cell=actor->GetParentCell();auto* world=cell?cell->GetbhkWorld():nullptr;if(!world)return false;
    auto from=actor->GetPosition();from.z+=48;target.z+=32;
    RE::bhkPickData ray{};const auto scale=RE::bhkWorld::GetWorldScaleInverse();
    ray.rayInput.from=RE::hkVector4(from*scale);ray.rayInput.to=RE::hkVector4(target*scale);
    ray.rayInput.filterInfo.SetCollisionLayer(RE::COL_LAYER::kLOS);ray.rayInput.enableShapeCollectionFilter=true;
    RE::BSReadLockGuard lock(world->worldLock);world->PickObject(ray);
    return !ray.rayOutput.HasHit()||ray.rayOutput.hitFraction>=.98f;
}
bool clearPickupReach(RE::Actor* actor,RE::TESObjectREFR* ground) {
    const auto origin=actor->GetPosition(),target=ground->Get3D()?ground->Get3D()->world.translate:ground->GetPosition();
    return actor->GetParentCell()==ground->GetParentCell()&&nearbyDrop(origin.GetDistance(target),target.z-origin.z); // Close recovery uses physical reach, not the unreliable floor-level LOS ray.
}
void pickupAtNPC(RE::Actor* actor,RE::TESObjectREFR* ground) {
    // Suppress the player-facing inventory sound; emit one spatial sound only
    // after the exact reference actually leaves the world.
    actor->PickUpObject(ground,1,false,false);
}
void recoveryPickupSound(RE::Actor* actor,RE::TESBoundObject* object) {
    RE::BGSSoundDescriptorForm* descriptor=nullptr;
    if(auto* armor=object?object->As<RE::TESObjectARMO>():nullptr)descriptor=armor->pickupSound;
    else if(auto* weapon=object?object->As<RE::TESObjectWEAP>():nullptr)descriptor=weapon->pickupSound;
    auto* output=RE::TESForm::LookupByID<RE::BGSSoundOutput>(0x99391); // SOMMono00400, inspected from Skyrim.esm
    auto* manager=RE::BSAudioManager::GetSingleton();
    if(!descriptor||!output||!manager||!actor->Get3D())return;
    RE::BSSoundHandle sound;
    if(manager->GetSoundHandle(sound,descriptor)){sound.SetOutputModel(output);sound.SetPosition(actor->GetPosition());sound.SetObjectToFollow(actor->Get3D());sound.Play();}
}
void recoverEquippedDrops() {
    // Only marked ground items are checked, once a second, after menus close.
    serviceRecoveryPauses();
    processDeferredDrops();
    // A saved cosmetic pause belongs to our dedicated package, never an arbitrary
    // mod's interrupt. Restore ordinary AI if its runtime lease did not survive load.
    for(const auto& drop:dropRecoveries)if(!recoveryPauses.contains(drop.victim))if(auto* actor=RE::TESForm::LookupByID<RE::Actor>(drop.victim);actor&&actor->GetCurrentPackage()==recoveryPausePackage&&recoveryPausePackage)actor->EndInterruptPackage(false);
    if(dropRecoveries.empty()){
        for(const auto& [id,path]:recoveryPaths)if(auto goal=path.goal.get()){cancelRecoveryWalk(RE::TESForm::LookupByID<RE::Actor>(id),goal.get());retireRecoveryGoal(goal.get(),id);}
        recoveryPaths.clear();equipRetries.clear();recoverySounds.clear();recoveryProgress.clear();return;
    }
    auto* ui=RE::UI::GetSingleton();
    if(playSeconds<nextRecovery||!ui||ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME))return;
    nextRecovery=playSeconds+1;
    std::set<RE::FormID> pendingReferences;
    for(const auto& drop:dropRecoveries)pendingReferences.insert(drop.reference);
    std::erase_if(equipRetries,[&](const auto& retry){return !pendingReferences.contains(retry.first);});
    std::erase_if(recoverySounds,[&](auto id){return !pendingReferences.contains(id);});
    std::erase_if(recoveryProgress,[&](const auto& entry){return !pendingReferences.contains(entry.first);});
    // The world reference is the authoritative identity of a dropped original.
    // Native drop/UID events can finish rebasing after the transfer hook returns.
    // Reconcile only this saved reference and base form; never select another
    // same-base item, and never relax matching for inventory stacks.
    for(auto& drop:dropRecoveries) {
        auto* ground=RE::TESForm::LookupByID<RE::TESObjectREFR>(drop.reference);
        if(!ground||ground->IsDeleted()||ground->IsMarkedForDeletion()||!ground->GetBaseObject()||ground->GetBaseObject()->GetFormID()!=drop.form)continue;
        auto* uid=ground->extraList.GetByType<RE::ExtraUniqueID>();
        if(!uid){ground->extraList.Add(new RE::ExtraUniqueID(drop.uidBase,drop.uid));ground->AddChange(RE::TESObjectREFR::ChangeFlags::kItemExtraData);uid=ground->extraList.GetByType<RE::ExtraUniqueID>();}
        if(!uid)continue;
        if(uid->baseID!=drop.uidBase||uid->uniqueID!=drop.uid){
            SKSE::log::info("Reconciled ground instance {:08X}: {:08X}:{:04X} -> {:08X}:{:04X}",drop.reference,drop.uidBase,drop.uid,uid->baseID,uid->uniqueID);
            for(auto& item:stolen)if(item.form==drop.form&&item.victim==drop.victim&&item.container==drop.reference){item.uidBase=uid->baseID;item.uid=uid->uniqueID;}
            drop.uidBase=uid->baseID;drop.uid=uid->uniqueID;
        }
    }
    std::erase_if(recoveryPaths,[&](const auto& pair){auto target=pair.second.target.get();const bool retire=pair.second.generation!=session.load()||!target||target->IsDeleted()||std::none_of(dropRecoveries.begin(),dropRecoveries.end(),[&](const DropRecovery& d){return d.victim==pair.first&&d.reference==target->GetFormID();});if(retire&&pair.second.generation==session.load())if(auto goal=pair.second.goal.get();goal){cancelRecoveryWalk(RE::TESForm::LookupByID<RE::Actor>(pair.first),goal.get());retireRecoveryGoal(goal.get(),pair.first);}return retire;});
    std::erase_if(dropRecoveries,[&](const DropRecovery& drop){
        auto* actor=RE::TESForm::LookupByID<RE::Actor>(drop.victim);
        if(!actor||actor->IsDead()||actor->IsDeleted())return true;
        if(!valid(actor)){if(auto it=recoveryProgress.find(drop.reference);it!=recoveryProgress.end())it->second.last=-1;return false;}
        Stolen key;key.form=drop.form;key.uidBase=drop.uidBase;key.uid=drop.uid;
        if(auto* extra=instance(actor,key)) {
            auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(drop.form);
            const bool weapon=object&&object->Is(RE::FormType::Weapon);
            auto& progress=recoveryProgress[drop.reference];
            if(recoverySounds.insert(drop.reference).second){recoveryPickupSound(actor,object);progress.dressAt=playSeconds+1.25;if(!actor->IsInCombat())pauseRecovery(actor,1.25);}
            const bool worn=extra->HasType<RE::ExtraWorn>()||extra->HasType<RE::ExtraWornLeft>();
            if(auto job=recoveryPaths.find(drop.victim);job!=recoveryPaths.end()) {
                auto target=job->second.target.get();
                if(!target||target->GetFormID()==drop.reference){if(auto goal=job->second.goal.get()){cancelRecoveryWalk(actor,goal.get());retireRecoveryGoal(goal.get(),drop.victim);}recoveryPaths.erase(job);}
            }
            if(!weapon&&actor->IsInCombat())return false;
            if(!worn){
                if(!weapon&&playSeconds<progress.dressAt)return false;
                auto& retry=equipRetries[drop.reference];if(playSeconds<retry.after)return false;
                ++retry.attempts;retry.after=playSeconds+(retry.attempts<=3?2:10);
                if(object)if(auto* equip=RE::ActorEquipManager::GetSingleton())equip->EquipObject(actor,object,extra,1,nullptr,true,true,false,false);
                SKSE::log::info("Recovery equip attempt {:08X}, item {:08X}: attempt {}, awaiting worn confirmation",drop.victim,drop.form,retry.attempts);
                return false;
            }
            equipRetries.erase(drop.reference);
            std::erase_if(stolen,[&](const Stolen& item){return item.form==drop.form&&item.uidBase==drop.uidBase&&item.uid==drop.uid;});
            SKSE::log::info("Equipped drop recovered {:08X}, item {:08X}; wearing confirmed",drop.victim,drop.form);return true;
        }
        auto* ground=RE::TESForm::LookupByID<RE::TESObjectREFR>(drop.reference);
        if(!ground||ground->IsDeleted()||ground->IsMarkedForDeletion()||ground->IsDisabled())return true;
        // The player or another actor already took this exact instance.
        for(const auto& tracked:stolen)if(tracked.form==drop.form&&tracked.uidBase==drop.uidBase&&tracked.uid==drop.uid&&tracked.container&&tracked.container!=drop.reference&&tracked.container!=drop.victim)return true;
        if(!ground->GetBaseObject()||ground->GetBaseObject()->GetFormID()!=drop.form)return true;
        const bool weapon=ground->GetBaseObject()->Is(RE::FormType::Weapon);
        auto* actorCell=actor->GetParentCell();auto* itemCell=ground->GetParentCell();
        const bool sameArea=witnessArea(actorCell&&actorCell==itemCell,actorCell&&itemCell&&!actorCell->IsInteriorCell()&&!itemCell->IsInteriorCell(),actor->GetWorldspace()&&actor->GetWorldspace()==ground->GetWorldspace());
        if(!ground->Is3DLoaded()||!sameArea){if(auto it=recoveryProgress.find(drop.reference);it!=recoveryProgress.end())it->second.last=-1;return false;}
        auto& progress=recoveryProgress[drop.reference];
        {
            const bool searching=!actor->IsInCombat()&&!actor->GetCurrentScene()&&actor->AsActorState()->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kIsSleeping;
            progress.advance(playSeconds,searching); // No search timeout: retain pending recovery.

        }
        // Repair pending drops from builds which assigned dynamic NPC bases.
        if(auto* owner=dropOwner(actor);owner&&ground->GetOwner()!=owner&&(!RE::PlayerCharacter::GetSingleton()||ground->GetOwner()!=RE::PlayerCharacter::GetSingleton()->GetActorBase())){assignDropOwner(ground,owner);for(auto& tracked:stolen)if(tracked.form==drop.form&&tracked.uidBase==drop.uidBase&&tracked.uid==drop.uid)tracked.owner=owner->GetFormID();SKSE::log::info("Repaired equipped ground owner {:08X}: {:08X}",drop.reference,owner->GetFormID());}
        if(actor->IsInCombat())if(auto job=recoveryPaths.find(drop.victim);job!=recoveryPaths.end()){if(auto goal=job->second.goal.get()){cancelRecoveryWalk(actor,goal.get());retireRecoveryGoal(goal.get(),drop.victim);}recoveryPaths.erase(job);}
        if(!weapon&&actor->IsInCombat())return false;
        if(actor->IsInCombat()) {
            // Combat weapons bypass clothing jobs, dialogue and walking callbacks.
            // Keep this local: forcing combat navigation is deliberately avoided.
            if(!clearPickupReach(actor,ground))return false;
            auto& retry=equipRetries[drop.reference];if(playSeconds<retry.after)return false;
            retry.after=playSeconds+2;
            pickupAtNPC(actor,ground);
            SKSE::log::info("Combat weapon pickup requested {:08X}, reference {:08X}",drop.victim,drop.reference);
            return false;
        }
        auto it=recoveryPaths.find(drop.victim);
        if(it==recoveryPaths.end()) {
            RecoveryPath path;path.target=ground->GetHandle();path.generation=session.load();path.ticket=fmt::format("{:X}-{:016X}",session.load(),std::uniform_int_distribution<std::uint64_t>{}(pressureRandom));path.sent=playSeconds-3;path.readyAt=0;it=recoveryPaths.emplace(drop.victim,path).first;
        }
        auto& path=it->second;
        if(path.target!=ground->GetHandle())return false; // one retrieval at a time per NPC
        const auto physicalPosition=ground->Get3D()?ground->Get3D()->world.translate:ground->GetPosition();
        const bool withinReach=clearPickupReach(actor,ground)||(path.finished&&path.reached&&nearbyDrop(actor->GetPosition().GetDistance(physicalPosition),physicalPosition.z-actor->GetPositionZ()));
        if(!withinReach&&playSeconds-path.lastGateLog>=5){path.lastGateLog=playSeconds;const auto physical=ground->Get3D()?ground->Get3D()->world.translate:ground->GetPosition();SKSE::log::info("Recovery blocked {:08X}: reference {:08X}, data-distance {:.1f}, physical-distance {:.1f}, vertical {:.1f}, exact-instance {}, claimed {}, finished {}, reached {}",drop.victim,drop.reference,actor->GetDistance(ground),actor->GetPosition().GetDistance(physical),physical.z-actor->GetPositionZ(),instance(ground,key)!=nullptr,path.claimed,path.finished,path.reached);}
        if(path.claimed&&!path.finished)path.walkRequired=!withinReach;
        if(path.finished&&!withinReach)if(auto goal=path.goal.get();goal&&goal->GetPosition().GetDistance(physicalPosition)>64){path.finished=false;path.claimed=false;path.dispatches=0;path.walkPrepared=false;path.walking=false;path.ticket=fmt::format("{:X}-{:016X}",session.load(),std::uniform_int_distribution<std::uint64_t>{}(pressureRandom));}
        // Native reach is sufficient after the pickup delay, even when a latent
        // PathToReference callback never returns. Invalidate its ticket on completion.
        if(!path.finished&&withinReach&&path.started&&path.walkPrepared&&playSeconds-path.sent>=2){path.finished=true;path.reached=true;path.readyAt=playSeconds;}
        if(recoveryWalkStalled(path.claimed,path.finished,actor->IsPathing(),playSeconds-path.sent)) {
            path.finished=true;path.reached=false;++path.failures;path.retryAfter=playSeconds+5;
            SKSE::log::warn("Recovery walk stalled {:08X}; no native path active, retrying safely",drop.victim);
        }
        if(path.finished&&path.failures>=3&&playSeconds>=path.retryAfter+30&&!actor->IsPathing())path.failures=0;
        if(!withinReach&&path.finished&&path.failures<3&&playSeconds>=path.retryAfter) {
            path.claimed=false;path.finished=false;path.dispatches=0;path.walkPrepared=false;path.walking=false;
            path.ticket=fmt::format("{:X}-{:016X}",session.load(),std::uniform_int_distribution<std::uint64_t>{}(pressureRandom));
        }
        if(!path.claimed&&path.dispatches>=3&&playSeconds-path.sent>=15){path.dispatches=0;path.ticket=fmt::format("{:X}-{:016X}",session.load(),std::uniform_int_distribution<std::uint64_t>{}(pressureRandom));}
        if(!path.claimed&&path.dispatches<3&&playSeconds-path.sent>=3) {
            auto marker=path.goal.get();
            if(!marker||marker->IsDeleted()){
                auto* markerBase=RE::TESForm::LookupByID<RE::TESBoundObject>(0x3B);
                marker=markerBase?actor->PlaceObjectAtMe(markerBase,false):RE::NiPointer<RE::TESObjectREFR>{};
                if(!marker){SKSE::log::error("Recovery navigation marker creation failed {:08X}",drop.victim);return false;}
                path.goal=marker->GetHandle();
            }
            const auto targetPosition=ground->Get3D()?ground->Get3D()->world.translate:ground->GetPosition();
            marker->MoveTo(ground);
            marker->SetPosition(targetPosition);
            SKSE::log::info("Recovery destination {:08X}: actor ({:.1f},{:.1f},{:.1f}), goal ({:.1f},{:.1f},{:.1f}), item ({:.1f},{:.1f},{:.1f})",drop.victim,actor->GetPositionX(),actor->GetPositionY(),actor->GetPositionZ(),marker->GetPositionX(),marker->GetPositionY(),marker->GetPositionZ(),targetPosition.x,targetPosition.y,targetPosition.z);
            std::erase_if(retiredGoals,[&](const RetiredGoal& old){return old.reference==marker->GetFormID();});
            path.walkRequired=!withinReach&&!actor->IsInCombat();path.sent=playSeconds;++path.dispatches;
            if(!path.started){releaseRecoveryPause(actor);actor->StopInteractingQuick(false);actor->NotifyAnimationGraph("IdleStop");}
            SKSE::ModCallbackEvent event{"PO_RecoverEquipped",path.ticket.c_str(),0,actor};
            if(auto* source=SKSE::GetModCallbackEventSource())source->SendEvent(&event);
            SKSE::log::info("Recovery walk requested {:08X}, reference {:08X}, ticket {}, dispatch {}",drop.victim,drop.reference,path.ticket,path.dispatches);
        }
        // A failed path is not proof that a nearby item cannot be picked up.
        // Cell, vertical separation and physical reach decide nearby pickup.
        if(!path.started||!path.walkPrepared||!path.finished||playSeconds<path.readyAt||!withinReach||playSeconds-path.lastPickup<2)return false;
        path.lastPickup=playSeconds;
        const bool visible=true; // nearby recovery intentionally does not require a floor-level LOS ray
        if(!canRecoverDrop(true,ground->Is3DLoaded(),false,false,actor->IsInCombat(),weapon,actor->GetPosition().GetDistance(ground->Get3D()?ground->Get3D()->world.translate:ground->GetPosition()),visible))return false;
        // Force collection only within local physical reach and the same cell.
        // Preserve the original reference; next tick verifies inventory.
        if(actor->GetParentCell()!=ground->GetParentCell()||ground->GetFormID()!=drop.reference||!ground->GetBaseObject()||ground->GetBaseObject()->GetFormID()!=drop.form)return false;
        const auto pickupDistance=actor->GetPosition().GetDistance(ground->Get3D()?ground->Get3D()->world.translate:ground->GetPosition());
        // Interrupt once per pickup attempt, after the pickup delay. Do not
        // wait on an idle or latent path callback to transfer a reachable item.
        if(auto goal=path.goal.get())cancelRecoveryWalk(actor,goal.get());
        releaseRecoveryPause(actor);
        actor->StopInteractingQuick(false);
        actor->NotifyAnimationGraph("IdleStop");
        pickupAtNPC(actor,ground);
        SKSE::log::info("Equipped drop pickup requested {:08X}, reference {:08X}, distance {:.1f}, path-finished {}, path-reached {}, failures {}",drop.victim,drop.reference,pickupDistance,path.finished,path.reached,path.failures);
        return false;
    });
}
using GroundTheftFn=void(*)(RE::Actor*,RE::TESObjectREFR*,RE::TESForm*,std::int32_t,std::int32_t,RE::TESForm*,bool);
GroundTheftFn originalGroundTheft=nullptr;
void droppedPickpocketCrime(RE::Actor* thief,RE::TESObjectREFR* reference,RE::TESForm* object,std::int32_t count,std::int32_t value,RE::TESForm* owner,bool warning) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    if(active&&thief==player&&reference){
        std::scoped_lock lock(mutex);
        auto found=std::find_if(dropRecoveries.begin(),dropRecoveries.end(),[&](const DropRecovery& drop){return drop.reference==reference->GetFormID()&&reference->GetBaseObject()&&reference->GetBaseObject()->GetFormID()==drop.form;});
        if(found!=dropRecoveries.end()&&thief->WouldBeStealing(reference)){
            if(reportedGroundPickups.contains(found->reference))return;
            auto* victim=RE::TESForm::LookupByID<RE::Actor>(found->victim);
            bool witnessed=valid(victim)&&sees(victim,player);
            if(victim&&!witnessed)for(auto handle:loaded())if(auto observer=handle.get();valid(observer.get())&&observer.get()!=player&&observer->GetDistance(reference)<900&&loyalWitness(observer.get(),victim)&&sees(observer.get(),player)&&clearNoiseRay(observer.get(),reference->GetPosition())){witnessed=true;break;}
            // Suppress the ordinary ground-theft alarm only for this exact mod drop.
            // Award the live pickpocket fine, without starting or stopping combat.
            if(witnessed&&victim){
                if(auto* faction=victim->GetCrimeFaction()){
                    auto* settings=RE::GameSettingCollection::GetSingleton();auto* setting=settings?settings->GetSetting("iCrimeGoldPickpocket"):nullptr;
                    if(setting){const auto fine=std::max(0,setting->data.i);faction->ModCrimeGold(fine,false);reportedGroundPickups.insert(found->reference);for(auto& tracked:stolen)if(tracked.form==found->form&&tracked.uidBase==found->uidBase&&tracked.uid==found->uid)tracked.reported=true;SKSE::log::info("Dropped item {:08X}: nonviolent pickpocket bounty {}, owner {:08X}; ground-theft combat alarm suppressed",found->reference,fine,found->victim);}
                    else SKSE::log::error("Dropped item pickpocket bounty unavailable: missing iCrimeGoldPickpocket");
                }
            }
            return;
        }
    }
    originalGroundTheft(thief,reference,object,count,value,owner,warning);
}
bool installDroppedCrimeHook(){
    try{
        REL::Relocation<std::uintptr_t> target{RELOCATION_ID(36427,37422)};auto& module=REL::Module::get();const auto base=module.base();
        std::ifstream input(std::filesystem::path(module.filePath().data()),std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Cannot open executable");const auto length=input.tellg();if(length<=0||length>512*1024*1024)throw std::runtime_error("Invalid executable size");
        std::vector<std::uint8_t> file(static_cast<std::size_t>(length));input.seekg(0);if(!input.read(reinterpret_cast<char*>(file.data()),static_cast<std::streamsize>(file.size())))throw std::runtime_error("Cannot read executable");
        auto sites=Hooks::discover(file,target.address()-base);if(sites.empty()||sites.size()>32)throw std::runtime_error("Unexpected ground-theft caller count");
        for(const auto& site:sites){const auto address=base+site.rva;const auto text=module.segment(REL::Segment::textx);if(address<text.address()+8||address+13>text.address()+text.size())throw std::runtime_error("Ground-theft caller outside executable code");const std::span<const std::uint8_t> live(reinterpret_cast<const std::uint8_t*>(address-8),21);if(!Hooks::contextMatches(file,site,live)||Hooks::destination(live,8,address)!=static_cast<std::int64_t>(target.address()))throw std::runtime_error("Ground-theft caller was modified");}
        originalGroundTheft=reinterpret_cast<GroundTheftFn>(target.address());for(const auto& site:sites)SKSE::GetTrampoline().write_call<5>(base+site.rva,droppedPickpocketCrime);
        SKSE::log::info("Installed scoped dropped-item pickpocket crime routing: {} verified callers",sites.size());return true;
    }catch(const std::exception& e){SKSE::log::error("Dropped-item crime routing unavailable: {}; new equipped drops disabled",e.what());equippedDropsReady=false;return false;}
}
struct Events final:RE::BSTEventSink<RE::TESContainerChangedEvent>,RE::BSTEventSink<RE::TESUniqueIDChangeEvent>,RE::BSTEventSink<RE::TESHitEvent>,RE::BSTEventSink<RE::MenuOpenCloseEvent>,RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    RE::FormID menuVictim=0;bool transferred=false;
    static Events& get(){static Events e;return e;}
    template<class F>void queue(F f){auto generation=session.load();if(auto* tasks=SKSE::GetTaskInterface())tasks->AddTask([f=std::move(f),generation]{if(active&&session==generation){std::scoped_lock lock(mutex);f();}});}
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* e,RE::BSTEventSource<RE::TESContainerChangedEvent>*)override {
        if(e&&active){
            {std::scoped_lock lock(mutex);if(pendingTransfer.observe(e->oldContainer,e->newContainer,e->baseObj,e->itemCount,playSeconds))accountPendingTransfer();}
            auto copy=*e;queue([this,copy]{
            trackContainerTheft(copy);
            auto dropped=copy.reference.get();
            for(auto& s:stolen)if(s.form==copy.baseObj&&s.uid==copy.uniqueID&&(s.container==copy.oldContainer||(copy.newContainer&&dropped&&s.container==dropped->GetFormID()))){advanceStoredTag(s,days());s.container=copy.newContainer;if(copy.newContainer!=0x14)if(auto* recipient=RE::TESForm::LookupByID<RE::Actor>(copy.newContainer);recipient&&!following(recipient))s.jurisdictionHidden=false;}
            if(copy.newContainer!=0x14||copy.oldContainer!=menuVictim||!menuVictim||copy.itemCount<=0)return;
            transferred=true;if(!stolenTracking.load())return;auto* victim=RE::TESForm::LookupByID<RE::Actor>(copy.oldContainer);auto* player=RE::PlayerCharacter::GetSingleton();if(!victim||!player)return;
            auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(copy.baseObj);if(!object)return;
            auto inv=player->GetInventory([&](RE::TESBoundObject& o){return o.GetFormID()==copy.baseObj;});RE::ExtraDataList* selected=nullptr;unsigned matches=0;
            for(auto& [o,data]:inv)if(data.second&&data.second->extraLists)for(auto* x:*data.second->extraLists)if(x&&x->GetOwner()==victim->GetActorBase()) {
                auto* uid=x->GetByType<RE::ExtraUniqueID>();
                if(copy.uniqueID?(uid&&uid->uniqueID==copy.uniqueID):(x->GetCount()==copy.itemCount)){selected=x;++matches;}
            }
            if(matches!=1||!selected){SKSE::log::info("Theft {:08X}: ambiguous instance; ownership left unchanged",copy.baseObj);return;}
            auto* uid=selected->GetByType<RE::ExtraUniqueID>();if(!uid){auto* changes=player->GetInventoryChanges();if(!changes)return;selected->Add(new RE::ExtraUniqueID(player->GetFormID(),changes->GetNextUniqueID()));uid=selected->GetByType<RE::ExtraUniqueID>();}if(!uid)return;
            Stolen s;s.victim=victim->GetFormID();s.form=copy.baseObj;s.owner=victim->GetActorBase()->GetFormID();s.container=0x14;s.uidBase=uid->baseID;s.uid=uid->uniqueID;s.value=std::uint32_t(std::max(0,object->GetGoldValue()));auto now=days();s.lastTagUpdate=now;s.originHold=currentHold(victim)?currentHold(victim)->GetFormID():0;s.stolenDay=now;s.noticed=now+(20+std::min(s.value,1000u)*.03)/86400;s.expires=now+memoryDays(s.value,config.tagDays);s.recognizesUntil=now+memoryDays(s.value,config.recognitionDays);
            if(std::none_of(stolen.begin(),stolen.end(),[&](const auto& old){return old.form==s.form&&old.uidBase==s.uidBase&&old.uid==s.uid;}))stolen.push_back(s);
        });}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESUniqueIDChangeEvent* e,RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*)override {
        if(e){auto copy=*e;queue([copy]{for(auto& d:deferredDrops)if(d.form==copy.objectID&&d.uidBase==copy.oldBaseID&&d.uid==copy.oldUniqueID){d.uidBase=copy.newBaseID;d.uid=copy.newUniqueID;}for(auto& d:dropRecoveries)if(d.form==copy.objectID&&d.uidBase==copy.oldBaseID&&d.uid==copy.oldUniqueID){d.uidBase=copy.newBaseID;d.uid=copy.newUniqueID;}for(auto& s:stolen)if(s.form==copy.objectID&&s.uidBase==copy.oldBaseID&&s.uid==copy.oldUniqueID){s.uidBase=copy.newBaseID;s.uid=copy.newUniqueID;}});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* e,RE::BSTEventSource<RE::TESHitEvent>*)override {
        if(e&&e->target){auto handle=e->target->GetHandle();queue([handle]{if(auto ref=handle.get())for(auto& h:loaded())if(auto a=h.get();a&&a->GetParentCell()==ref->GetParentCell()&&a->GetDistance(ref.get())<900)if(clearNoiseRay(a.get(),ref->GetPosition()))commotions[a->GetFormID()].add(Noise::Hit,10,playSeconds,8);});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* e,RE::BSTEventSource<RE::MenuOpenCloseEvent>*)override {
        if(e&&e->menuName==RE::StatsMenu::MENU_NAME&&!e->opening&&capManaged.load())queue([]{resolveMaximum(RE::PlayerCharacter::GetSingleton());});
        if(e&&e->menuName==RE::ContainerMenu::MENU_NAME){auto open=e->opening;queue([this,open]{if(open&&RE::ContainerMenu::GetContainerMode()==RE::ContainerMenu::ContainerMode::kPickpocket){auto ref=RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());auto* a=ref?ref->As<RE::Actor>():nullptr;menuVictim=a?a->GetFormID():0;transferred=false;if(menuVictim){auto& s=states[menuVictim];interaction(s,playSeconds,openingPenalty.load());SKSE::log::info("Inventory interaction {:08X}: opening-penalty {}, suspicion {:.1f}",menuVictim,openingPenalty.load(),s.suspicion);}}else if(!open){menuVictim=0;witnessDisplayCache={};}});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* e,RE::BSTEventSource<RE::BSAnimationGraphEvent>*)override {
        if(e&&e->holder){const auto id=e->holder->GetFormID();const std::string tag=e->tag.c_str();if(tag!="SoundPlay.NPCHumanEat"&&tag!="SoundPlay.NPCHumanDrink")return RE::BSEventNotifyControl::kContinue;queue([id,tag]{auto* a=RE::TESForm::LookupByID<RE::Actor>(id);auto* p=RE::PlayerCharacter::GetSingleton();if(!valid(a)||!p)return;if(tag=="SoundPlay.NPCHumanEat"||tag=="SoundPlay.NPCHumanDrink")commotions[id].add(Noise::Meal,8,playSeconds,5);});}return RE::BSEventNotifyControl::kContinue;
    }
};
void applyExplosion(RE::NiPoint3 position,RE::FormID cellID,RE::FormID worldID,float radius) {
    for(auto handle:loaded())if(auto actor=handle.get();actor&&actor->AsActorState()->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kIsSleeping) {
        auto* cell=actor->GetParentCell();if(!cell)continue;
        if(cell->GetFormID()!=cellID&&(cell->IsInteriorCell()||!worldID||!actor->GetWorldspace()||actor->GetWorldspace()->GetFormID()!=worldID))continue;
        const float distance=actor->GetPosition().GetDistance(position);
        if(distance>=radius)continue;
        const auto strength=noiseAtDistance(distance,radius,20,clearNoiseRay(actor.get(),position));
        if(strength>0)commotions[actor->GetFormID()].add(Noise::Explosion,strength,playSeconds,6);
    }
}
template<unsigned Index> struct ExplosionSignal {
    using Fn=void(*)(RE::Explosion*);static inline Fn previous=nullptr;
    static void initialize(RE::Explosion* explosion) {
        previous(explosion);
        if(!active||!explosion)return;
        auto* cell=explosion->GetParentCell();if(!cell)return;
        const auto position=explosion->GetPosition();if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))return;const auto cellID=cell->GetFormID();
        const auto worldID=explosion->GetWorldspace()?explosion->GetWorldspace()->GetFormID():0;
        const auto raw=explosion->GetExplosionRuntimeData().radius;
        const float radius=std::clamp(std::isfinite(raw)?raw*2.f:900.f,400.f,1200.f);
        Events::get().queue([position,cellID,worldID,radius]{applyExplosion(position,cellID,worldID,radius);});
    }
    static void install(REL::VariantID id) {
        REL::Relocation<std::uintptr_t> table{id};
        const auto target=*reinterpret_cast<std::uintptr_t*>(table.address()+0xA2*sizeof(void*));
        if(!executableAddress(target))throw std::runtime_error("Explosion initializer is not executable");
        previous=reinterpret_cast<Fn>(table.write_vfunc(0xA2,initialize));
    }
};
void installExplosionSignals() {
    try{ExplosionSignal<0>::install(RE::VTABLE_Explosion[0]);ExplosionSignal<1>::install(RE::VTABLE_ChainExplosion[0]);SKSE::log::info("Native explosion/chain-explosion distraction hooks installed");}
    catch(const std::exception& error){SKSE::log::error("Explosion distraction hook unavailable: {}",error.what());}
}
void sampleCommotions(const std::vector<RE::ActorHandle>& actors,RE::Actor* player) {
    std::set<RE::FormID> nowFleeing;
    for(const auto& handle:actors)if(auto actor=handle.get();valid(actor.get())&&actor->GetDistance(player)<1800) {
        const auto id=actor->GetFormID();
        const bool fleeing=nativeCondition(actor.get(),RE::FUNCTION_DATA::FunctionID::kIsFleeing);
        if(fleeing)nowFleeing.insert(id);
        if(actor->IsInCombat())commotions[id].add(Noise::Combat,10,playSeconds,4);
        auto* scene=actor->GetCurrentScene();
        const bool argument=scene&&scene->isPlaying&&!scene->isShuttingDown&&argumentScenes.contains(scene->GetFormID());
        if(argument)commotions[id].add(Noise::Argument,14,playSeconds,1);
        const bool panicStarted=fleeing&&!fleeingActors.contains(id);
        if(!argument&&!panicStarted)continue;
        for(const auto& observerHandle:actors)if(auto observer=observerHandle.get();valid(observer.get())&&observer.get()!=actor.get()&&observer->AsActorState()->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kIsSleeping) {
            const auto* cell=observer->GetParentCell();const auto* sourceCell=actor->GetParentCell();
            if(!witnessArea(cell&&cell==sourceCell,cell&&sourceCell&&!cell->IsInteriorCell()&&!sourceCell->IsInteriorCell(),observer->GetWorldspace()&&observer->GetWorldspace()==actor->GetWorldspace()))continue;
            const auto distance=observer->GetDistance(actor.get());
            const float radius=argument?600.f:450.f;
            if(distance>=radius||std::abs(observer->GetHeadingAngle(actor->GetPosition(),false))>100||!witnessSight(observer.get(),actor.get()))continue;
            const auto strength=noiseAtDistance(distance,radius,argument?8.f:6.f,true);
            commotions[observer->GetFormID()].add(argument?Noise::Argument:Noise::Panic,strength,playSeconds,argument?1:2);
        }
    }
    fleeingActors=std::move(nowFleeing);
    std::erase_if(commotions,[&](const auto& entry){return entry.second.expired(playSeconds);});
}
void tick() {
    auto now=Clock::now();float delta=std::chrono::duration<float>(now-lastTick).count();lastTick=now;
    auto* ui=RE::UI::GetSingleton();if(!active||!ui||ui->GameIsPaused())return;
    std::scoped_lock lock(mutex);if(capManaged.load())resolveMaximum(RE::PlayerCharacter::GetSingleton());playSeconds+=std::clamp(delta,0.f,2.f);auto actors=loaded();auto* p=RE::PlayerCharacter::GetSingleton();if(!p)return;
    observeBumps(actors,p);if(playSeconds<nextFullTick)return;nextFullTick=playSeconds+.5;reapRecoveryGoals();recoverEquippedDrops();sampleCommotions(actors,p);
    for(auto& h:actors)if(auto a=h.get();a&&a->GetParentCell()==p->GetParentCell()&&a->GetDistance(p)<1200){auto& s=states[a->GetFormID()];decay(s,playSeconds);sampleReaction(attentionTrails[a->GetFormID()],reactionFacts(a.get(),p),playSeconds);if(s.bumpUntil<=playSeconds&&p->IsSneaking()&&sees(a.get(),p))s.observedUntil=playSeconds+30;if(animated.insert(a->GetFormID()).second&&!a->AddAnimationGraphEventSink(&Events::get()))animated.erase(a->GetFormID());}
    double today=days();std::erase_if(bumpRules,[&](const auto& entry){const auto& r=entry.second;return r.nextBonusDay<=today&&r.blockedUntilDay<=today&&r.recognizedUntilDay<=today&&r.bonusUntil<=playSeconds;});std::set<std::pair<std::uint32_t,std::uint16_t>> completed;std::set<RE::FormID> reportedVictims;
    for(auto& item:stolen)advanceStoredTag(item,today);
    if(!stolenTracking.load())return; // History is retained; disabling never clears vanilla ownership.
    for(auto& s:stolen){auto* container=RE::TESForm::LookupByID<RE::TESObjectREFR>(s.container);if(!container)continue;auto* x=instance(container,s);if(!x)continue;
        const bool onGround=container->GetBaseObject()&&container->GetBaseObject()->GetFormID()==s.form;
        if(!onGround&&(stolenTagExpiration.load()||!x->GetOwner()||x->GetOwner()->GetFormID()!=s.owner)&&today>=std::max(s.expires,s.recognizesUntil))completed.emplace(s.uidBase,s.uid);
        if(s.victim&&today>=s.noticed&&today<s.recognizesUntil)states[s.victim].suspicion=std::max(15.f,states[s.victim].suspicion);
        if(stolenTagExpiration.load()&&!onGround&&!evidenceContainers.contains(s.container)){
            if(today>=s.expires){if(x->GetOwner()&&x->GetOwner()->GetFormID()==s.owner)x->SetOwner(p->GetActorBase());}
            else if(betterGuard.load()&&s.container==0x14&&s.originHold){
                auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(s.form);auto* hold=currentHold(p);
                // Unknown locations retain the original tag rather than guessing jurisdiction.
                if(object&&hold){const bool identifiable=guardCanIdentify(&s,hold,object,s.value);auto* owner=x->GetOwner();
                    if(!identifiable&&owner&&owner->GetFormID()==s.owner){x->SetOwner(p->GetActorBase());s.jurisdictionHidden=true;}
                    else if(identifiable&&s.jurisdictionHidden&&owner==p->GetActorBase())if(auto* originalOwner=RE::TESForm::LookupByID(s.owner)){x->SetOwner(originalOwner);s.jurisdictionHidden=false;}
                }
            }
        }
        if(s.reported||today<s.noticed||today>=s.recognizesUntil||s.container!=0x14||!(x->HasType<RE::ExtraWorn>()||x->HasType<RE::ExtraWornLeft>()))continue;
        auto* victim=RE::TESForm::LookupByID<RE::Actor>(s.victim);if(!valid(victim)||following(victim)||!sees(victim,p))continue;
        if(std::abs(victim->GetHeadingAngle(p->GetPosition(),false))>=100||!witnessSight(victim,p))continue;

        if(!recognitionAlarm)continue;
        s.reported=true;states[s.victim].suspicion=60;
        // Several worn possessions recognized together produce one crime report.
        if(reportedVictims.insert(s.victim).second){
            auto* faction=victim->GetCrimeFaction();const auto before=faction?faction->GetCrimeGoldNonViolent():0;
            // Match the vanilla failed-attempt call exactly: player, victim,
            // no transferred object, count zero. The engine selects crime 1.
            recognitionAlarm(p,victim,nullptr,0);
            const auto after=faction?faction->GetCrimeGoldNonViolent():0;
            SKSE::log::info("Victim {:08X} recognized stolen item {:08X}: native pickpocket reaction, crime faction {:08X}, nonviolent bounty {} -> {}",s.victim,s.form,faction?faction->GetFormID():0,before,after);
        }else SKSE::log::info("Victim {:08X} also recognized {:08X}; same recognition report, no second alarm",s.victim,s.form);
    }
    std::erase_if(stolen,[&](const auto& s){return completed.contains({s.uidBase,s.uid});});
}
constexpr std::uint32_t record=0x504F5331,trailRecord=0x504F5431,pressureRecord=0x504F5031,karmaRecord=0x504F4B31,bumpRuleRecord=0x504F4231,dropRecord=0x504F4431,retiredRecord=0x504F4731,searchRecord=0x504F5231,unfastenRecord=0x504F5531,pauseRecord=0x504F4831;
void save(SKSE::SerializationInterface* api){std::scoped_lock lock(mutex);for(auto& item:stolen)advanceStoredTag(item,days());if(!api->OpenRecord(record,3))return;api->WriteRecordData(&playSeconds,sizeof(playSeconds));auto count=std::uint32_t(states.size());api->WriteRecordData(&count,4);for(auto& [id,s]:states){api->WriteRecordData(&id,4);api->WriteRecordData(&s,sizeof(s));}count=std::uint32_t(stolen.size());api->WriteRecordData(&count,4);for(auto& s:stolen)api->WriteRecordData(&s,sizeof(s));




if(api->OpenRecord(pauseRecord,1)){count=static_cast<std::uint32_t>(recoveryPauses.size());api->WriteRecordData(&count,4);for(const auto& [id,pause]:recoveryPauses)api->WriteRecordData(&id,4);}
if(api->OpenRecord(unfastenRecord,1)){count=static_cast<std::uint32_t>(deferredDrops.size());api->WriteRecordData(&count,4);for(const auto& drop:deferredDrops){for(auto id:{drop.victim,drop.form,drop.uidBase})api->WriteRecordData(&id,4);api->WriteRecordData(&drop.uid,2);api->WriteRecordData(&drop.after,8);}}
if(api->OpenRecord(searchRecord,1)){count=static_cast<std::uint32_t>(recoveryProgress.size());api->WriteRecordData(&count,4);for(const auto& [id,progress]:recoveryProgress){api->WriteRecordData(&id,4);api->WriteRecordData(&progress.spent,8);}}
if(api->OpenRecord(trailRecord,3)){count=0;for(const auto& [id,trail]:attentionTrails)if(trail.reaction>0||trail.wariness>0||(states.contains(id)&&states.at(id).bumpUntil>playSeconds))++count;api->WriteRecordData(&count,4);for(const auto& [id,trail]:attentionTrails)if(trail.reaction>0||trail.wariness>0||(states.contains(id)&&states.at(id).bumpUntil>playSeconds)){api->WriteRecordData(&id,4);api->WriteRecordData(&trail.last,sizeof(trail.last));api->WriteRecordData(&trail.reaction,sizeof(trail.reaction));api->WriteRecordData(&trail.bumpType,sizeof(trail.bumpType));api->WriteRecordData(&trail.bumpEffect,sizeof(trail.bumpEffect));api->WriteRecordData(&trail.wariness,sizeof(trail.wariness));api->WriteRecordData(&trail.warinessLast,sizeof(trail.warinessLast));}}
if(api->OpenRecord(bumpRuleRecord,1)){count=static_cast<std::uint32_t>(bumpRules.size());api->WriteRecordData(&count,4);for(const auto& [id,r]:bumpRules){api->WriteRecordData(&id,4);for(const auto* value:{&r.nextBonusDay,&r.blockedUntilDay,&r.recognizedUntilDay,&r.bonusUntil})api->WriteRecordData(value,8);}}
if(api->OpenRecord(retiredRecord,1)){std::map<RE::FormID,RetiredGoal> goals;for(const auto& goal:retiredGoals)goals[goal.reference]=goal;for(const auto& [owner,path]:recoveryPaths)if(auto goal=path.goal.get();goal)goals[goal->GetFormID()]={goal->GetFormID(),owner,playSeconds+30};const auto total=static_cast<std::uint32_t>(goals.size());api->WriteRecordData(&total,4);for(const auto& [id,goal]:goals){api->WriteRecordData(&goal.reference,4);api->WriteRecordData(&goal.owner,4);api->WriteRecordData(&goal.after,8);}}
if(api->OpenRecord(karmaRecord,3)){api->WriteRecordData(&karma.credit,4);}
if(api->OpenRecord(dropRecord,2)){count=static_cast<std::uint32_t>(dropRecoveries.size());api->WriteRecordData(&count,4);for(const auto& d:dropRecoveries){for(const auto* id:{&d.victim,&d.reference,&d.form,&d.uidBase})api->WriteRecordData(id,4);api->WriteRecordData(&d.uid,2);}}
if(api->OpenRecord(pressureRecord,1)){count=0;for(const auto& [id,p]:theftPressure)if(p.stacks&&p.until>playSeconds)++count;api->WriteRecordData(&count,4);for(const auto& [id,p]:theftPressure)if(p.stacks&&p.until>playSeconds){api->WriteRecordData(&id,4);api->WriteRecordData(&p.stacks,4);api->WriteRecordData(&p.until,sizeof(p.until));}}}
void clear(SKSE::SerializationInterface*){active=false;++session;std::scoped_lock lock(mutex);loadSettings();timerPreview=-1;karma=BadLuckProtection{};bumpRules.clear();dropRecoveries.clear();deferredDrops.clear();recoveryPaths.clear();equipRetries.clear();recoverySounds.clear();recoveryProgress.clear();recoveryPauses.clear();reportedGroundPickups.clear();commotions.clear();fleeingActors.clear();retiredGoals.clear();witnessSightCache.clear();nextRecovery=0;equippedRequest={};states.clear();attentionTrails.clear();theftPressure.clear();pendingTransfer={};pendingTicket=0;stolen.clear();animated.clear();bumpObservations.clear();havePlayerPosition=false;nextFullTick=0;playSeconds=0;cachedCap=-1;witnessDisplayCache={};debugValues.clear();debugNames.clear();Events::get().menuVictim=0;}
void load(SKSE::SerializationInterface* api){clear(api);std::uint32_t type,version,length;while(api->GetNextRecordInfo(type,version,length)){if(type==pauseRecord&&version==1){std::uint32_t count=0;if(api->ReadRecordData(&count,4)!=4||count>10000||length!=4+count*4)return;for(unsigned i=0;i<count;++i){RE::FormID id=0,resolved=0;if(api->ReadRecordData(&id,4)!=4)return;if(recoveryPausePackage&&api->ResolveFormID(id,resolved))recoveryPauses[resolved]={reinterpret_cast<std::uintptr_t>(recoveryPausePackage),0};}continue;}if(type==unfastenRecord&&version==1){std::uint32_t count=0;if(api->ReadRecordData(&count,4)!=4||count>10000||length!=4+count*22)return;for(unsigned i=0;i<count;++i){DeferredDrop drop;bool validIDs=true;for(auto* id:{&drop.victim,&drop.form,&drop.uidBase}){RE::FormID resolved=0;if(api->ReadRecordData(id,4)!=4)return;if(!api->ResolveFormID(*id,resolved))validIDs=false;*id=resolved;}if(api->ReadRecordData(&drop.uid,2)!=2||api->ReadRecordData(&drop.after,8)!=8)return;if(validIDs&&std::isfinite(drop.after))deferredDrops.push_back(drop);}continue;}if(type==searchRecord&&version==1){std::uint32_t count=0;if(api->ReadRecordData(&count,4)!=4||count>10000||length!=4+count*12)return;for(unsigned i=0;i<count;++i){std::uint32_t id=0,resolved=0;double spent=0;if(api->ReadRecordData(&id,4)!=4||api->ReadRecordData(&spent,8)!=8)return;if(std::isfinite(spent)&&spent>=0&&spent<=45&&api->ResolveFormID(id,resolved))recoveryProgress[resolved].spent=spent;}continue;}if(type==retiredRecord&&version==1){std::uint32_t count;if(api->ReadRecordData(&count,4)!=4||count>10000||length!=4+16*count)return;for(unsigned i=0;i<count;++i){RetiredGoal goal;std::uint32_t ref,owner;if(api->ReadRecordData(&ref,4)!=4||api->ReadRecordData(&owner,4)!=4||api->ReadRecordData(&goal.after,8)!=8)return;if(std::isfinite(goal.after)&&api->ResolveFormID(ref,goal.reference)&&api->ResolveFormID(owner,goal.owner))retiredGoals.push_back(goal);}continue;}if(type==dropRecord&&(version==1||version==2)){
std::uint8_t ignored=0;std::uint32_t count=0;if((version==1&&(api->ReadRecordData(&ignored,1)!=1||ignored>1))||api->ReadRecordData(&count,4)!=4||count>10000||length!=(version==1?5u:4u)+18*count){SKSE::log::error("Invalid equipped-drop co-save record");return;} // Legacy toggle intentionally ignored: the INI wins.
for(unsigned i=0;i<count;++i){DropRecovery d;bool resolved=true;for(auto* id:{&d.victim,&d.reference,&d.form,&d.uidBase}){std::uint32_t old;if(api->ReadRecordData(&old,4)!=4)return;if(!old||!api->ResolveFormID(old,*id))resolved=false;}if(api->ReadRecordData(&d.uid,2)!=2)return;if(resolved)dropRecoveries.push_back(d);}continue;
}if(type==bumpRuleRecord&&version==1){std::uint32_t count;if(api->ReadRecordData(&count,4)!=4||count>100000||length!=4+count*36)return;for(unsigned i=0;i<count;++i){std::uint32_t id,resolved;BumpRules r;if(api->ReadRecordData(&id,4)!=4)return;for(auto* value:{&r.nextBonusDay,&r.blockedUntilDay,&r.recognizedUntilDay,&r.bonusUntil})if(api->ReadRecordData(value,8)!=8)return;if(r.valid()&&api->ResolveFormID(id,resolved))bumpRules[resolved]=r;}continue;}if(type==karmaRecord){
if(version==3){BadLuckProtection saved;if(length==4&&api->ReadRecordData(&saved.credit,4)==4&&saved.valid())karma=saved;else SKSE::log::error("Invalid bad-luck karma co-save record");}
else if(version==2){BadLuckProtection saved;std::uint8_t enabled=0;if(length==5&&api->ReadRecordData(&saved.credit,4)==4&&api->ReadRecordData(&enabled,1)==1&&enabled<=1&&saved.valid()){karma=saved;}else SKSE::log::error("Invalid bad-luck karma co-save record");}
else if(version==1){KarmaRolls legacy;std::uint8_t enabled=0;if(length==113&&api->ReadRecordData(&legacy.seed,8)==8&&api->ReadRecordData(&legacy.cursor,4)==4&&api->ReadRecordData(legacy.bag.data(),100)==100&&api->ReadRecordData(&enabled,1)==1&&enabled<=1&&legacy.valid()){karma={};SKSE::log::info("Migrated shuffled karma to bad-luck protection; INI preference retained, initialized credit to zero");}else SKSE::log::error("Invalid legacy karma record");}continue;}if(type==pressureRecord&&version==1){std::uint32_t count;if(api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){std::uint32_t id,resolved;TheftPressure pressure;if(api->ReadRecordData(&id,4)!=4||api->ReadRecordData(&pressure.stacks,4)!=4||api->ReadRecordData(&pressure.until,sizeof(pressure.until))!=sizeof(pressure.until))return;if(api->ResolveFormID(id,resolved)&&pressure.stacks>0&&pressure.stacks<=100&&std::isfinite(pressure.until))theftPressure[resolved]=pressure;}continue;}if(type==trailRecord&&(version>=1&&version<=3)){std::uint32_t count;if(api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){std::uint32_t id,resolved;AttentionTrail trail;if(api->ReadRecordData(&id,4)!=4||api->ReadRecordData(&trail.last,sizeof(trail.last))!=sizeof(trail.last)||api->ReadRecordData(&trail.reaction,sizeof(trail.reaction))!=sizeof(trail.reaction))return;if(version>=2&&api->ReadRecordData(&trail.bumpType,sizeof(trail.bumpType))!=sizeof(trail.bumpType))return;if(version>=3&&(api->ReadRecordData(&trail.bumpEffect,sizeof(trail.bumpEffect))!=sizeof(trail.bumpEffect)||api->ReadRecordData(&trail.wariness,sizeof(trail.wariness))!=sizeof(trail.wariness)||api->ReadRecordData(&trail.warinessLast,sizeof(trail.warinessLast))!=sizeof(trail.warinessLast)))return;if(api->ResolveFormID(id,resolved)&&std::isfinite(trail.bumpEffect)&&trail.bumpEffect>=.25f&&trail.bumpEffect<=1&&std::isfinite(trail.wariness)&&trail.wariness>=0&&trail.wariness<=3&&std::isfinite(trail.warinessLast)&&trail.bumpType<=1&&std::isfinite(trail.last)&&std::isfinite(trail.reaction)&&trail.reaction>=0&&trail.reaction<=57)attentionTrails[resolved]=trail;}continue;}if(type!=record||(version<1||version>3))continue;std::uint32_t count;if(api->ReadRecordData(&playSeconds,sizeof(playSeconds))!=sizeof(playSeconds)||!std::isfinite(playSeconds)||api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){std::uint32_t id,newID;AttentionState s;if(api->ReadRecordData(&id,4)!=4||api->ReadRecordData(&s,sizeof(s))!=sizeof(s))return;if(api->ResolveFormID(id,newID)&&std::isfinite(s.suspicion))states.emplace(newID,s);}if(api->ReadRecordData(&count,4)!=4||count>100000)return;for(unsigned i=0;i<count;++i){Stolen s;const auto bytes=version==1?offsetof(Stolen,lastTagUpdate):version==2?offsetof(Stolen,originHold):sizeof(s);if(api->ReadRecordData(&s,static_cast<std::uint32_t>(bytes))!=bytes)return;bool validIDs=true;for(auto* id:{&s.victim,&s.form,&s.owner,&s.container,&s.uidBase,&s.originHold}){std::uint32_t next=0;if(*id&&!api->ResolveFormID(*id,next)){validIDs=false;break;}if(*id)*id=next;}if(validIDs&&std::isfinite(s.expires)&&std::isfinite(s.recognizesUntil)&&std::isfinite(s.lastTagUpdate))stolen.push_back(s);}}}
void message(SKSE::MessagingInterface::Message* e){switch(e->type){case SKSE::MessagingInterface::kPostLoad:{connectLootCycles();auto h=GetModuleHandleW(L"StandingPickpocket.dll");if(h){auto request=reinterpret_cast<StandingPickpocketAPI::Request>(GetProcAddress(h,"StandingPickpocket_RequestAPI"));if(request)standing=request(1);}distanceOverride();break;}
case SKSE::MessagingInterface::kDataLoaded:{auto* data=RE::TESDataHandler::GetSingleton();recoveryPausePackage=data->LookupForm<RE::TESPackage>(0x803,"PickpocketOverhaul.esp");if(!recoveryPausePackage)SKSE::log::warn("Recovery pause package missing; update PickpocketOverhaul.esp with DLL and scripts");capPerk=data->LookupForm<RE::BGSPerk>(0x1B,"HandtoHand.esp");artfulManaged=data->LookupModByName("Artful Dodger - Dynamic Pickpocket Cap.esp")!=nullptr;capManaged=artfulManaged||capPerk||data->LookupModByName("Adamant.esp");if(capManaged.load()){resolveMaximum(RE::PlayerCharacter::GetSingleton());SKSE::log::info("Simonrim maximum enabled: normal 89/99; first qualifying bump 90/100; Hand to Hand perk {:08X}",capPerk?capPerk->GetFormID():0);}auto faction=[data](unsigned id){return data->LookupForm<RE::TESFaction>(id,"Skyrim.esm");};follower=faction(0x5C84E);loadCompatibility(data);auto* source=RE::ScriptEventSourceHolder::GetSingleton();auto& events=Events::get();initializeContraband(data);installGuardRemoval();source->AddEventSink<RE::TESContainerChangedEvent>(&events);source->AddEventSink<RE::TESUniqueIDChangeEvent>(&events);source->AddEventSink<RE::TESHitEvent>(&events);RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);hookReady=installChance();if(hookReady){installAttempts();prepareRecognitionAlarm();installDroppedCrimeHook();installWitnessDisplay();installTimerPreview();installExplosionSignals();}SKSE::log::info("Native small/large bump monitor enabled at 100ms; greetings and sleeping handling enabled");worker=std::jthread([](std::stop_token stop){while(!stop.stop_requested()){std::this_thread::sleep_for(std::chrono::milliseconds(100));if(active&&!queued.exchange(true)){auto generation=session.load();SKSE::GetTaskInterface()->AddTask([generation]{queued=false;if(active&&session==generation)tick();});}}});break;}
case SKSE::MessagingInterface::kPreLoadGame:{std::scoped_lock lock(mutex);recoveryPaths.clear();active=false;++session;break;}
case SKSE::MessagingInterface::kNewGame:clear(nullptr);[[fallthrough]];
case SKSE::MessagingInterface::kPostLoadGame:if(e->type==SKSE::MessagingInterface::kPostLoadGame&&!e->data)break;lastTick=Clock::now();if(capManaged.load())resolveMaximum(RE::PlayerCharacter::GetSingleton());active=hookReady.load();distanceOverride();break;default:break;}}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse){SKSE::Init(skse);auto folder=SKSE::log::log_directory();if(folder){*folder/="PickpocketOverhaul.log";auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(folder->string(),true);auto logger=std::make_shared<spdlog::logger>("global",sink);spdlog::set_default_logger(logger);spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);}SKSE::log::info("Pickpocket Chance Rework 1.0.0; runtime {}",REL::Module::get().version().string());loadSettings();auto* api=SKSE::GetSerializationInterface();api->SetUniqueID(0x504F5631);api->SetSaveCallback(save);api->SetLoadCallback(load);api->SetRevertCallback(clear);if(!SKSE::GetPapyrusInterface()->Register(registerPapyrus))return false;return SKSE::GetMessagingInterface()->RegisterListener(message);}

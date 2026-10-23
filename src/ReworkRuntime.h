// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
// Included in Plugin.cpp's anonymous namespace, after the common runtime helpers.
std::atomic_bool betterGuard=true;
bool contrabandPreference=true,externalContraband=false;std::set<RE::FormID> contrabandForms,evidenceContainers;
unsigned localValuable=100,veryValuable=1000;float recentTheftDays=1;
const LootCycleAPI::V1* lootCycles=nullptr;
bool getBetterGuard(RE::StaticFunctionTag*){return betterGuard.load();}
void setBetterGuard(RE::StaticFunctionTag*,bool value){betterGuard=value;WritePrivateProfileStringW(L"Settings",L"BetterGuardRemoval",value?L"1":L"0",configPath);}
std::string capSource(RE::StaticFunctionTag*){return artfulManaged?"Artful Dodger":capManaged.load()?"Adamant / Hand to Hand":"None";}
void loadReworkSettings(){
 contrabandPreference=GetPrivateProfileIntW(L"Settings",L"ContrabandHandling",1,configPath)!=0;
 betterGuard=GetPrivateProfileIntW(L"Settings",L"BetterGuardRemoval",1,configPath)!=0;
 localValuable=std::clamp(GetPrivateProfileIntW(L"Settings",L"LocalValuableGold",100,configPath),1u,1000000u);
 veryValuable=std::clamp(GetPrivateProfileIntW(L"Settings",L"VeryValuableGold",1000,configPath),localValuable,1000000u);
 recentTheftDays=std::clamp(float(GetPrivateProfileIntW(L"Settings",L"RecentTheftDays",1,configPath)),0.f,30.f);
}
void initializeContraband(RE::TESDataHandler* data){
 contrabandForms.clear();evidenceContainers.clear();externalContraband=false;
 for(auto* faction:data->GetFormArray<RE::TESFaction>())if(faction&&faction->crimeData.factionStolenContainer)evidenceContainers.insert(faction->crimeData.factionStolenContainer->GetFormID());
 for(unsigned i=0;i<data->GetLoadedModCount();++i)if(auto* file=data->GetLoadedMods()[i];file&&contrabandProviderName(file->fileName)){externalContraband=true;break;}
 for(unsigned i=0;i<data->GetLoadedLightModCount();++i)if(auto* file=data->GetLoadedLightMods()[i];file&&contrabandProviderName(file->fileName)){externalContraband=true;break;}
 for(const auto& record:contrabandRecords)if(auto* form=data->LookupForm(record.localID,record.plugin))contrabandForms.insert(form->GetFormID());
 SKSE::log::info("Contraband: {} resolved forms; enabled {}; external Contraband Confiscation {}. Other guard features remain active",contrabandForms.size(),contrabandEnabled(contrabandPreference,externalContraband),externalContraband);
}
std::string contrabandProvider(RE::StaticFunctionTag*){return externalContraband?"Contraband Confiscation":contrabandPreference?"This mod":"Disabled in INI";}
bool contraband(RE::TESBoundObject* object){return object&&contrabandEnabled(contrabandPreference,externalContraband)&&contrabandForms.contains(object->GetFormID());}
const Stolen* trackedInstance(RE::TESBoundObject* object,RE::ExtraDataList* x){
 auto* uid=x?x->GetByType<RE::ExtraUniqueID>():nullptr;if(!uid)return nullptr;
 for(const auto& s:stolen)if(s.form==object->GetFormID()&&s.uidBase==uid->baseID&&s.uid==uid->uniqueID)return &s;
 return nullptr;
}
RE::TESFaction* currentHold(RE::TESObjectREFR* actor){if(!actor)return nullptr;auto* loc=actor->GetCurrentLocation();for(unsigned i=0;loc&&i<32;++i,loc=loc->parentLoc)if(loc->unreportedCrimeFaction)return loc->unreportedCrimeFaction;auto* a=actor->As<RE::Actor>();return a?a->GetCrimeFaction():nullptr;}
bool guardCanIdentify(const Stolen* s,RE::TESFaction* hold,RE::TESBoundObject* object,unsigned value){
 const bool local=s&&hold&&s->originHold==hold->GetFormID();
 const bool recent=s&&s->stolenDay>=0&&days()>=s->stolenDay&&days()-s->stolenDay<recentTheftDays;
 return guardRecognizes(contraband(object),value,local,recent,localValuable,veryValuable);
}
using PayFineFn=void(*)(RE::PlayerCharacter*,RE::TESFaction*,bool,bool);
PayFineFn previousPayFine=nullptr;
void selectivePayFine(RE::PlayerCharacter* player,RE::TESFaction* hold,bool jail,bool remove){
 if(!betterGuard.load()||!remove||!hold||!hold->crimeData.factionStolenContainer){previousPayFine(player,hold,jail,remove);return;}
 std::scoped_lock lock(mutex);
 struct Seize{RE::TESBoundObject* object;RE::ExtraDataList* extra;int count;};std::vector<Seize> seized;
 auto inventory=player->GetInventory();
 for(auto& [object,data]:inventory){
  if(!object||data.first<=0||!data.second||data.second->IsQuestObject())continue;
  auto* entry=data.second.get();const unsigned value=unsigned(std::max(0,entry->GetValue()));int listed=0;
  if(entry->extraLists)for(auto* x:*entry->extraLists)if(x){
   const int count=std::min(std::max(0,int(x->GetCount())),std::max(0,data.first-listed));listed+=count;if(!count)continue;
   auto* owner=x->GetOwner();const bool stolenOwner=owner&&!entry->IsOwnedBy(player,owner,true);
   if(contraband(object)||(stolenOwner&&guardCanIdentify(trackedInstance(object,x),hold,object,value)))seized.push_back({object,x,count});
  }
  const int plain=std::max(0,data.first-listed);
  if(plain&&(contraband(object)||(!entry->IsOwnedBy(player,true)&&guardCanIdentify(nullptr,hold,object,value))))seized.push_back({object,nullptr,plain});
 }
 for(auto item:seized){
  // Legal contraband must not retain an explicit player-ownership exemption in
  // the evidence chest. Existing stolen-owner identity remains untouched.
  if(contraband(item.object)&&item.extra){auto* owner=item.extra->GetOwner();if(!owner||owner==player->GetActorBase())item.extra->SetOwner(hold);}
  player->RemoveItem(item.object,item.count,RE::ITEM_REMOVE_REASON::kStoreInContainer,item.extra,hold->crimeData.factionStolenContainer);
 }
 SKSE::log::info("Selective guard confiscation: hold {:08X}, {} item stacks",hold->GetFormID(),seized.size());
 previousPayFine(player,hold,jail,false);
}
void installGuardRemoval(){
 REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
 previousPayFine=reinterpret_cast<PayFineFn>(table.write_vfunc(0xBB,selectivePayFine));
 SKSE::log::info("Selective guard confiscation installed on PlayerCharacter::PayFine");
}
void connectLootCycles(){auto module=GetModuleHandleW(L"PickpocketingLoot.dll");if(!module)return;auto request=reinterpret_cast<LootCycleAPI::Request>(GetProcAddress(module,"PickpocketingLoot_RequestCycleAPI"));if(request)lootCycles=request(1);if(lootCycles)SKSE::log::info("Dynamic Pickpocket Loot cycle API v1 connected");}

void trackContainerTheft(const RE::TESContainerChangedEvent& event){
 if(!stolenTracking||event.newContainer!=0x14||event.itemCount<=0)return;
 auto ground=event.reference.get();auto* source=event.oldContainer?RE::TESForm::LookupByID<RE::TESObjectREFR>(event.oldContainer):ground.get();if(!source||source->As<RE::Actor>())return;
 auto* player=RE::PlayerCharacter::GetSingleton();auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(event.baseObj);if(!player||!object)return;
 auto inv=player->GetInventory([&](RE::TESBoundObject& o){return &o==object;});RE::ExtraDataList* selected=nullptr;unsigned matches=0;int value=object->GetGoldValue();
 for(auto& [o,data]:inv)if(data.second&&data.second->extraLists)for(auto* x:*data.second->extraLists)if(x&&x->GetOwner()&&!data.second->IsOwnedBy(player,x->GetOwner(),true)){
  auto* uid=x->GetByType<RE::ExtraUniqueID>();if(event.uniqueID?(uid&&uid->uniqueID==event.uniqueID):(x->GetCount()==event.itemCount)){selected=x;++matches;value=data.second->GetValue();}
 }
 if(matches!=1||!selected)return;
 auto* uid=selected->GetByType<RE::ExtraUniqueID>();if(!uid){auto* changes=player->GetInventoryChanges();if(!changes)return;selected->Add(new RE::ExtraUniqueID(player->GetFormID(),changes->GetNextUniqueID()));uid=selected->GetByType<RE::ExtraUniqueID>();}if(!uid)return;
 if(trackedInstance(object,selected))return;
 Stolen s;s.form=object->GetFormID();s.owner=selected->GetOwner()->GetFormID();s.container=0x14;s.uidBase=uid->baseID;s.uid=uid->uniqueID;s.value=unsigned(std::max(0,value));s.stolenDay=s.lastTagUpdate=days();s.noticed=days();s.expires=days()+memoryDays(s.value,config.tagDays);s.recognizesUntil=s.expires;
 auto* hold=currentHold(source);if(!hold)hold=currentHold(player);s.originHold=hold?hold->GetFormID():0;stolen.push_back(s);
}


// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
// Included in Plugin.cpp's anonymous namespace, after the common runtime helpers.
std::atomic_bool betterGuard=true;
bool contrabandPreference=true,externalContraband=false;std::set<RE::FormID> contrabandForms,evidenceContainers;
unsigned localValuable=20,valuableStolen=100,veryValuable=300;bool importStolenPending=true;
const LootCycleAPI::V1* lootCycles=nullptr;
bool getBetterGuard(RE::StaticFunctionTag*){return betterGuard.load();}
void setBetterGuard(RE::StaticFunctionTag*,bool value){betterGuard=value;WritePrivateProfileStringW(L"Settings",L"BetterGuardRemoval",value?L"1":L"0",configPath);}
std::string capSource(RE::StaticFunctionTag*){return artfulManaged?"Artful Dodger":capManaged.load()?"Adamant / Hand to Hand":"None";}
void loadReworkSettings(){
 contrabandPreference=GetPrivateProfileIntW(L"Settings",L"ContrabandHandling",1,configPath)!=0;
 betterGuard=GetPrivateProfileIntW(L"Settings",L"BetterGuardRemoval",1,configPath)!=0;
 localValuable=std::clamp(GetPrivateProfileIntW(L"Settings",L"MinimumConfiscationGold",20,configPath),0u,10000u);
 veryValuable=std::clamp(GetPrivateProfileIntW(L"Settings",L"HighValueStolenGold",300,configPath),localValuable,10000u);
 valuableStolen=std::clamp(GetPrivateProfileIntW(L"Settings",L"ValuableStolenGold",100,configPath),localValuable,veryValuable);
 // The stolen-tag expiry is the only local confiscation timer.
}
float getGuardThreshold(int index){return float(index==11?veryValuable:index==12?valuableStolen:localValuable);}
void setGuardThreshold(int index,float value){
 const auto gold=static_cast<unsigned>(std::clamp(value,0.f,10000.f));
 if(index==11)veryValuable=std::max(gold,valuableStolen);
 else if(index==12)valuableStolen=std::clamp(gold,localValuable,veryValuable);
 else localValuable=std::min(gold,valuableStolen);
 WritePrivateProfileStringW(L"Settings",L"HighValueStolenGold",std::to_wstring(veryValuable).c_str(),configPath);
 WritePrivateProfileStringW(L"Settings",L"ValuableStolenGold",std::to_wstring(valuableStolen).c_str(),configPath);
 WritePrivateProfileStringW(L"Settings",L"MinimumConfiscationGold",std::to_wstring(localValuable).c_str(),configPath);
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
 const bool recent=!s||!stolenTagExpiration.load()||days()<s->expires;
 return guardRecognizes(contraband(object),value,local,recent,localValuable,veryValuable,valuableStolen);
}
// Import legacy vanilla theft stacks once per load; unique IDs make this idempotent.
void importExistingStolen(RE::PlayerCharacter* player){
 if(!importStolenPending||!stolenTracking.load()||!player)return;
 auto* hold=currentHold(player);auto* changes=player->GetInventoryChanges();
 if(!hold||!changes)return; // Retry when a hold can be resolved.
 importStolenPending=false;unsigned imported=0;const auto now=days();
 auto inventory=player->GetInventory();
 for(auto& [object,data]:inventory){
  if(!object||data.first<=0||!data.second||data.second->IsQuestObject())continue;
  auto* entry=data.second.get();std::vector<RE::ExtraDataList*> lists;int listed=0;
  if(entry->extraLists)for(auto* x:*entry->extraLists)if(x){listed+=std::max(0,int(x->GetCount()));lists.push_back(x);}
  // Base-owned stacks may have no extra data yet. Attach identity to the real
  // inventory-change entry, never to GetInventory's temporary copy.
  auto* inheritedOwner=entry->GetOwner();int plain=std::max(0,data.first-listed);
  if(plain&&inheritedOwner&&!entry->IsOwnedBy(player,inheritedOwner,true)){
   RE::InventoryEntryData* live=nullptr;
   if(changes->entryList)for(auto* e:*changes->entryList)if(e&&e->object==object){live=e;break;}
   if(!live){live=new RE::InventoryEntryData(object,0);changes->AddEntryData(live);}
   while(plain>0){auto* x=allocateRuntimeExtraData<RE::ExtraDataList>([](std::size_t bytes){return RE::malloc(bytes);},[](void* storage){::new(storage) RE::ExtraDataList();});if(!x){SKSE::log::error("Stolen inventory migration allocation failed; retry on next load");importStolenPending=true;return;}auto count=std::min(plain,32767);x->SetCount(static_cast<std::uint16_t>(count));x->SetOwner(inheritedOwner);live->AddExtraList(x);lists.push_back(x);plain-=count;}
  }
  for(auto* x:lists){
   auto* owner=x->GetOwner();if(!owner||entry->IsOwnedBy(player,owner,true)||trackedInstance(object,x))continue;
   auto* uid=x->GetByType<RE::ExtraUniqueID>();if(!uid){x->Add(new RE::ExtraUniqueID(player->GetFormID(),changes->GetNextUniqueID()));uid=x->GetByType<RE::ExtraUniqueID>();}if(!uid)continue;
   RE::InventoryEntryData single(object,x->GetCount());single.AddExtraList(x);
   Stolen s;s.form=object->GetFormID();s.owner=owner->GetFormID();s.container=0x14;s.uidBase=uid->baseID;s.uid=uid->uniqueID;s.value=unsigned(std::max(0,single.GetValue()));
   s.originHold=hold->GetFormID();s.stolenDay=s.noticed=s.lastTagUpdate=now;s.expires=now+memoryDays(s.value,config.tagDays);s.recognizesUntil=now+memoryDays(s.value,config.recognitionDays);
   stolen.push_back(s);++imported;
  }
 }
 if(imported)player->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
 // Existing tracked instances keep their age; legacy records acquire a hold.
 for(auto& s:stolen)if(s.container==0x14&&!s.originHold)s.originHold=hold->GetFormID();
 // Apply jurisdiction/value rules immediately, not at the next periodic tick.
 if(stolenTagExpiration.load()&&betterGuard.load())for(auto& s:stolen)if(s.container==0x14&&s.originHold){
  auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(s.form);auto* x=instance(player,s);if(!object||!x)continue;
  if(now>=s.expires||(s.value<localValuable&&!contraband(object))){if(auto* owner=x->GetOwner();owner&&owner->GetFormID()==s.owner){x->SetOwner(player->GetActorBase());s.jurisdictionHidden=now<s.expires;}}
 }
 SKSE::log::info("Imported {} existing stolen stacks; origin hold {:08X}; existing tracked timers retained",imported,hold->GetFormID());
}
using PayFineFn=void(*)(RE::PlayerCharacter*,RE::TESFaction*,bool,bool);
PayFineFn previousPayFine=nullptr;
void selectivePayFine(RE::PlayerCharacter* player,RE::TESFaction* hold,bool jail,bool remove){
 if(!betterGuard.load()||!remove||!hold||!hold->crimeData.factionStolenContainer){previousPayFine(player,hold,jail,remove);return;}
 std::scoped_lock lock(mutex);
 importExistingStolen(player);
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
  // Read the instance name before removal invalidates its extra data. Only
  // announce items actually removed, using the game's localized HUD helper.
  RE::InventoryEntryData displayEntry(item.object,item.count);
  if(item.extra)displayEntry.AddExtraList(item.extra);
  const std::string displayName=displayEntry.GetDisplayName();
  const auto before=player->GetItemCount(item.object);
  player->RemoveItem(item.object,item.count,RE::ITEM_REMOVE_REASON::kStoreInContainer,item.extra,hold->crimeData.factionStolenContainer);
  const auto removed=std::clamp(before-player->GetItemCount(item.object),0,item.count);
  if(removed)RE::SendHUDMessage::ShowInventoryChangeMessage(item.object,removed,false,false,displayName.c_str());
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


// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#include "QuickPocketAPI.h"
#include "QuickPocketHookMemory.h"
#include "QuickPocketDispatch.h"
#include <bcrypt.h>
#include <fstream>
#include <spdlog/sinks/basic_file_sink.h>
extern "C" const PCRQuickPocketAPI::V1* PickpocketOverhaul_RequestQuickPocketAPI(std::uint32_t);
namespace PCRQPIntegration {
using namespace PCRQuickPocketMemory;
const PCRQuickPocketAPI::V1* pcr=nullptr;
bool installed=false;
using PreviewFn=std::int32_t(*)(RE::Actor*,RE::InventoryEntryData*);
using AttemptFn=bool(*)(RE::PlayerCharacter*,RE::TESObjectREFR*,RE::InventoryEntryData*,std::int32_t,bool);
PreviewFn originalPreview=nullptr;AttemptFn originalAttempt=nullptr;
RE::ObjectRefHandle (*getVictim)()=nullptr;bool (*plantMode)()=nullptr;
std::string fileHash(HMODULE module){
 std::array<wchar_t,32768> path{};auto n=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));if(!n||n>=path.size())return {};
 std::ifstream file(std::filesystem::path(path.data()),std::ios::binary);if(!file)return {};
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
 DWORD size=0,used=0;BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&used,0);
 std::vector<unsigned char> object(size);std::array<unsigned char,32> result{};bool ok=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0;
 std::array<char,65536> block{};
 while(ok&&file){file.read(block.data(),block.size());auto count=file.gcount();if(count)ok=BCryptHashData(hash,reinterpret_cast<PUCHAR>(block.data()),static_cast<ULONG>(count),0)>=0;}
 ok=ok&&file.eof()&&BCryptFinishHash(hash,result.data(),static_cast<ULONG>(result.size()),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
 if(!ok)return {};std::string hex;for(auto byte:result){hex+="0123456789abcdef"[byte>>4];hex+="0123456789abcdef"[byte&15];}return hex;
}
bool moduleMatches(HMODULE module,const char* hash,std::uint32_t stamp,std::uint32_t size){
 if(!module||fileHash(module)!=hash)return false;
 const auto base=reinterpret_cast<std::uintptr_t>(module);auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
 auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 return nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&nt->FileHeader.TimeDateStamp==stamp&&nt->OptionalHeader.SizeOfImage==size;
}

void beginCurrent(){if(!pcr||!pcr->Ready()||!getVictim)return;if(auto ref=getVictim().get())if(auto* actor=ref->As<RE::Actor>())pcr->Begin(actor);}
std::int32_t preview(RE::Actor* victim,RE::InventoryEntryData* entry){
 if(!pcr||!pcr->Ready()||!entry)return originalPreview(victim,entry);
 return PCRQP::preview(pcr,originalPreview,victim,entry,plantMode()?1:std::max(1,entry->countDelta));
}
bool attempt(RE::PlayerCharacter* player,RE::TESObjectREFR* victim,RE::InventoryEntryData* entry,std::int32_t count,bool taking){
 if(!pcr||!pcr->Ready())return originalAttempt(player,victim,entry,count,taking);
 return PCRQP::attempt(pcr,originalAttempt,player,victim,entry,count,taking);
}
struct Menus final:RE::BSTEventSink<RE::MenuOpenCloseEvent>{
 RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override{
  if(event&&pcr&&event->menuName=="LootMenu"){if(event->opening)beginCurrent();else pcr->End();}
  return RE::BSEventNotifyControl::kContinue;
 }
}menus;
void* nearJump(std::uintptr_t site,std::uintptr_t target){
 SYSTEM_INFO info{};GetSystemInfo(&info);const auto unit=std::uintptr_t(info.dwAllocationGranularity);const auto center=site&~(unit-1);
 for(std::uintptr_t distance=unit;distance<0x70000000;distance+=unit)for(int direction:{1,-1}){
  const auto address=direction>0?center+distance:(center>distance?center-distance:0);if(!address)continue;
  auto* code=VirtualAlloc(reinterpret_cast<void*>(address),32,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!code)continue;
  const auto bytes=jumpTo(target);std::memcpy(code,bytes.data(),14);DWORD old=0;
  if(!VirtualProtect(code,32,PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return nullptr;}FlushInstructionCache(GetCurrentProcess(),code,14);return code;
 }return nullptr;
}
void install(){
 auto qp=GetModuleHandleW(L"QuickPocket.dll");
 if(!qp){SKSE::log::info("QuickPocket not installed; built-in compatibility inactive");return;}
 if(GetModuleHandleW(L"PCRQuickPocket.dll")){SKSE::log::warn("Legacy PCRQuickPocket.dll detected: remove the separate adapter and restart to use PCR 1.1.0 built-in compatibility");return;}
 pcr=PickpocketOverhaul_RequestQuickPocketAPI(1);
 if(!pcr||pcr->version!=1){SKSE::log::error("Built-in QuickPocket bridge API unavailable");return;}
 if(!moduleMatches(qp,"4875a22611f4f3f8d229bc6818f5628d4ba26f925cca0fd3883be547891a93f7",0x6aa694c4,0xbe000)){SKSE::log::warn("Built-in QuickPocket compatibility inactive: unsupported QuickPocket binary");return;}
 auto base=reinterpret_cast<std::uintptr_t>(qp);
 const std::array<std::uint8_t,16> prefix{0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x60};
 const std::array<std::uint8_t,5> call{0xe8,0xab,0x4d,0x01,0x00};
 if(std::memcmp(reinterpret_cast<void*>(base+0x11ca0),prefix.data(),prefix.size())||std::memcmp(reinterpret_cast<void*>(base+0x28df0),call.data(),call.size())){SKSE::log::warn("Built-in QuickPocket compatibility inactive: hook bytes changed");return;}
 originalPreview=reinterpret_cast<PreviewFn>(originalStub(base+0x11ca0,16));
 originalAttempt=reinterpret_cast<AttemptFn>(base+0x28df0+5+0x14dab);
 auto* relay=nearJump(base+0x28df0,reinterpret_cast<std::uintptr_t>(attempt));
 if(!originalPreview||!relay){SKSE::log::error("Built-in QuickPocket compatibility inactive: trampoline allocation failed");return;}
 auto jump=jumpTo(reinterpret_cast<std::uintptr_t>(preview));std::vector<std::uint8_t> entry(jump.begin(),jump.end());entry.push_back(0x90);
 std::vector<std::uint8_t> invoke(5);invoke[0]=0xe8;const auto relative=static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(relay))-static_cast<std::int64_t>(base+0x28df0+5);
 if(relative<INT32_MIN||relative>INT32_MAX)return;const auto displacement=static_cast<std::int32_t>(relative);std::memcpy(invoke.data()+1,&displacement,4);
 getVictim=reinterpret_cast<RE::ObjectRefHandle(*)()>(base+0x22dc0);plantMode=reinterpret_cast<bool(*)()>(base+0x22e90);
 std::vector<Patch> patches{{base+0x11ca0,{prefix.begin(),prefix.end()},entry},{base+0x28df0,{call.begin(),call.end()},invoke}};
 if(!apply(patches)){SKSE::log::error("Built-in QuickPocket compatibility inactive: patches rejected");return;}
 installed=true;
 SKSE::log::info("Built-in QuickPocket compatibility installed: PCR previews, rolls, karma, opening/failure/stack pressure; original QuickPocket transfers; no Standing dependency");
}
void message(SKSE::MessagingInterface::Message* m){
 if(m->type==SKSE::MessagingInterface::kPostPostLoad)install();
 if(m->type==SKSE::MessagingInterface::kDataLoaded&&installed)RE::UI::GetSingleton()->AddEventSink(&menus);
 if(installed&&pcr&&(m->type==SKSE::MessagingInterface::kPreLoadGame||m->type==SKSE::MessagingInterface::kNewGame||m->type==SKSE::MessagingInterface::kPostLoadGame))pcr->End();
}
}

#include "QuickPocketHookMemory.h"
#include <iostream>
#include <stdexcept>
int replacement(){return 84;}
int main(){using namespace PCRQuickPocketMemory;int checks=0;auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("Native hook regression");};
// Same relocation-free prologue as the supported QuickPocket eligibility routine.
const std::vector<std::uint8_t> code{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x40,
0xb8,42,0,0,0,0x48,0x8b,0x5c,0x24,0x58,0x48,0x8b,0x74,0x24,0x60,0x48,0x83,0xc4,0x40,0x5f,0xc3};
auto* page=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(page!=nullptr);std::memcpy(page,code.data(),code.size());DWORD old;check(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)!=0);FlushInstructionCache(GetCurrentProcess(),page,code.size());
using Fn=int(*)();auto fn=reinterpret_cast<Fn>(page);check(fn()==42);auto* stub=originalStub(reinterpret_cast<std::uintptr_t>(page));check(stub!=nullptr);auto original=reinterpret_cast<Fn>(stub);check(original()==42);
auto jump=jumpTo(reinterpret_cast<std::uintptr_t>(replacement));std::vector<Patch> patches{{reinterpret_cast<std::uintptr_t>(page),{code.begin(),code.begin()+15},{jump.begin(),jump.end()}}};
auto rejected=patches;rejected[0].expected[0]^=1;check(!apply(rejected));check(fn()==42);
check(apply(patches));check(fn()==84);check(original()==42);check(!apply(patches));
std::vector<Patch> rollback{{reinterpret_cast<std::uintptr_t>(page),{jump.begin(),jump.end()},{code.begin(),code.begin()+15}}};check(apply(rollback));check(fn()==42);
VirtualFree(stub,0,MEM_RELEASE);VirtualFree(page,0,MEM_RELEASE);
// The preview hook has a 16-byte prologue, not the 15-byte default.
const std::vector<std::uint8_t> previewCode{0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x60,
0xb8,42,0,0,0,0x48,0x8b,0xac,0x24,0x80,0,0,0,0x48,0x8b,0xb4,0x24,0x88,0,0,0,0x48,0x83,0xc4,0x60,0x41,0x5e,0xc3};
page=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(page!=nullptr);std::memcpy(page,previewCode.data(),previewCode.size());check(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)!=0);FlushInstructionCache(GetCurrentProcess(),page,previewCode.size());
fn=reinterpret_cast<Fn>(page);check(fn()==42);stub=originalStub(reinterpret_cast<std::uintptr_t>(page),16);check(stub!=nullptr);original=reinterpret_cast<Fn>(stub);check(original()==42);
std::vector<std::uint8_t> jump16(jump.begin(),jump.end());jump16.push_back(0x90);patches={{reinterpret_cast<std::uintptr_t>(page),{previewCode.begin(),previewCode.begin()+16},jump16}};check(apply(patches));check(fn()==84);check(original()==42);VirtualFree(stub,0,MEM_RELEASE);VirtualFree(page,0,MEM_RELEASE);
std::cout<<"Passed "<<checks<<" native hook smoke checks; not Skyrim gameplay\n";
}

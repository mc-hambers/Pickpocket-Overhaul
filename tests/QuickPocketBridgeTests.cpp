#include "QuickPocketDispatch.h"
#include <iostream>
#include <stdexcept>
bool ready=true,result=false;int apiRolls=0,ordinaryRolls=0,apiPreviews=0,ordinaryPreviews=0,display=64,lastCount=0;bool lastTaking=false;
bool isReady(){return ready;}void begin(RE::Actor*){}void end(){}
int preview(RE::Actor*,RE::InventoryEntryData*,int count){++apiPreviews;lastCount=count;return display;}
bool roll(RE::PlayerCharacter*,RE::TESObjectREFR*,RE::InventoryEntryData*,int count,bool taking){++apiRolls;lastCount=count;lastTaking=taking;return result;}
int fallbackPreview(RE::Actor*,RE::InventoryEntryData*){++ordinaryPreviews;return 90;}
bool fallbackRoll(RE::PlayerCharacter*,RE::TESObjectREFR*,RE::InventoryEntryData*,int,bool){++ordinaryRolls;return true;}
int main(){int checks=0;auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("Bridge delegation regression");};PCRQuickPocketAPI::V1 api{1,isReady,begin,end,preview,roll};
check(PCRQP::preview(&api,fallbackPreview,nullptr,nullptr,20)==64);check(apiPreviews==1&&ordinaryPreviews==0&&lastCount==20);
check(!PCRQP::attempt(&api,fallbackRoll,nullptr,nullptr,nullptr,20,true));check(apiRolls==1&&ordinaryRolls==0&&lastCount==20&&lastTaking);
result=true;check(PCRQP::attempt(&api,fallbackRoll,nullptr,nullptr,nullptr,1,false));check(apiRolls==2&&ordinaryRolls==0&&!lastTaking);
ready=false;check(PCRQP::attempt(&api,fallbackRoll,nullptr,nullptr,nullptr,1,true));check(ordinaryRolls==1&&apiRolls==2);
check(PCRQP::preview(&api,fallbackPreview,nullptr,nullptr,1)==90);check(ordinaryPreviews==1&&apiPreviews==1);
ready=true;display=-1;check(PCRQP::preview(&api,fallbackPreview,nullptr,nullptr,1)==90);check(ordinaryPreviews==2&&apiPreviews==2);
check(PCRQP::attempt(nullptr,fallbackRoll,nullptr,nullptr,nullptr,1,true));check(ordinaryRolls==2&&apiRolls==2);
std::cout<<"Passed "<<checks<<" bridge routing checks; gameplay untested\n";}

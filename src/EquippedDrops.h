// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
namespace PO {
inline bool matchesEquippedTransfer(std::uint32_t victim,std::uint32_t form,std::uintptr_t instance,std::uint64_t generation,std::uint32_t source,std::uint32_t object,std::uintptr_t extra,std::uint64_t current) {
    return victim&&form&&instance&&victim==source&&form==object&&instance==extra&&generation==current;
}
inline bool shouldDropEquipped(bool enabled,bool ready,bool success,bool taking,bool sleeping,bool equipped,bool supported,int count) {
    return enabled&&ready&&success&&taking&&!sleeping&&equipped&&supported&&count==1;
}
inline bool nearbyDrop(float distance,float vertical) {
    return std::isfinite(distance)&&std::isfinite(vertical)&&distance>=0&&distance<=270&&std::abs(vertical)<=64;
}
inline bool canRecoverDrop(bool alive,bool loaded,bool menuOpen,bool sleeping,bool combat,bool weapon,float distance,bool lineOfSight) {
    return alive&&loaded&&!menuOpen&&!sleeping&&(!combat||weapon)&&std::isfinite(distance)&&distance<=270&&lineOfSight;
}
struct RecoveryProgress {
    double spent=0,last=-1,pickupAt=0,dressAt=0;bool animationStarted=false;
    bool advance(double now,bool eligible) {
        if(!eligible||!std::isfinite(now)){last=-1;return false;}
        if(last>=0)spent=std::min(45.0,spent+std::clamp(now-last,0.0,2.0));
        last=now;return false; // Pending recovery never expires while the original item exists.
    }
};
struct DropRecovery {
    std::uint32_t victim=0,reference=0,form=0,uidBase=0;
    std::uint16_t uid=0;
};
}

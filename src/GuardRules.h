// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
namespace PO {
enum class GuardGoods { Ignored, LocalStolen, LocalValuable, HighValue, Contraband };
inline GuardGoods guardGoods(bool contraband,unsigned value,bool local,bool tagActive,unsigned minimum=20,unsigned exceptional=300,unsigned valuable=100) {
 if(contraband)return GuardGoods::Contraband;
 if(value<minimum||!tagActive)return GuardGoods::Ignored;
 if(value>exceptional)return GuardGoods::HighValue;
 if(!local)return GuardGoods::Ignored;
 return value>=valuable?GuardGoods::LocalValuable:GuardGoods::LocalStolen;
}
inline bool guardRecognizes(bool contraband,unsigned value,bool local,bool tagActive,unsigned minimum=20,unsigned exceptional=300,unsigned valuable=100) {
 return guardGoods(contraband,value,local,tagActive,minimum,exceptional,valuable)!=GuardGoods::Ignored;
}
}

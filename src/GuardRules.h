// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
namespace PO {
inline bool guardRecognizes(bool contraband,unsigned value,bool local,bool recent,unsigned valuable=100,unsigned exceptional=1000) {
 return contraband||value>=exceptional||(local&&(recent||value>=valuable));
}
}

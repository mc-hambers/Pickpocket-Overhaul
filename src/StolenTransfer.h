// Copyright (C) 2026 Maxmc. GPL-3.0-or-later.
#pragma once
#include "Core.h"
namespace PO {
struct StolenTransfer {
 std::uint32_t form=0,source=0,destination=0,ground=0,uidBase=0;
 std::uint16_t uid=0;
 bool hasFullIdentity=false;
 bool matches(const Stolen& s) const {
  const auto from=source?source:ground;
  if(!form||!from||s.form!=form||s.container!=from)return false;
  if(hasFullIdentity)return s.uidBase==uidBase&&s.uid==uid;
  return uid&&s.uid==uid;
 }
 std::uint32_t target() const {return destination?destination:ground;}
};
}

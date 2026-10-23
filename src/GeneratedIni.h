// Copyright (C) 2026 Maxmc. GPL-3.0-or-later. See LICENSE.txt.
#pragma once
#include <string_view>
namespace PO {
inline constexpr std::string_view initialSettings = R"POINI(; Portable MCM settings: copy this file to another setup, then load a save or restart.
; MCM changes are written here immediately. This file overrides saved preferences.
; Internal factor strengths are fixed by the mod.
; Old AwarenessPoints/DistractionPoints/WeightPoints/ValuePoints/ProfilePoints keys are ignored.
[Settings]
; Bad-luck-only protection; only character history is stored in the co-save.
KarmaMode=1
InventoryOpeningPenalty=1
; Disable recognition/loss tracking without changing vanilla theft ownership.
StolenItemTracking=1
; Automatic stolen-tag expiration; requires tracking enabled.
StolenTagExpiration=1
; Optional original-instance drops; only pending recovery is stored in the co-save.
EquippedItemDrops=0
BetterGuardRemoval=1
; Only our contraband classification; auto-disabled when Contraband Confiscation is loaded.
ContrabandHandling=1
PointSkew=1
; Guard recognition thresholds, gold per item.
LocalValuableGold=100
VeryValuableGold=1000
RecentTheftDays=1
WitnessStrength=1
StolenTagDays=3
AbsoluteMaximum=97
RecognitionDays=1
DistanceMultiplier=0.45

[Debug]
; Show the unsigned witness cap effect beside the normal chance.
Enabled=0
; Testing override; bypass all percentage caps and penalties.
Force100Percent=0
)POINI";
}

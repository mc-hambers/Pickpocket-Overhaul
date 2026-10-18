# 0.1.18 skill ceiling balance

The author rejected 0.1.17's inexperience subtraction and selected stronger skill contribution to the situational ceiling. Restored 0.1.16 execution and all prior features, then changed the starting ceiling to maximum*(1-.4*pow(1-trained,1.5)). Default maximum 98 gives skill 0/15/50/100 starting ceilings approximately 58.8/67.28/84.14/98. Existing situational adjustments and bounded light-item skill relief remain. No post-perk novice subtraction.

Reconstructed reported log cases yield approximately 40% and 26%. Regression fixtures only; runtime behavior still requires verification. No new saved-state migration, Papyrus or MCM fields.

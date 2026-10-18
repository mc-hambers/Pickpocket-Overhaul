# 0.1.8 sleep relief derived from negated attention
Supersedes the interrupted general-awareness execution change; it was not published. Awake execution remains the 0.1.7 formula.

For sleeping targets, compute the ordinary awareness loss twice: same observed situation treated as awake versus sleeping. Sleep relief is max(0, awake loss minus sleeping loss), then added to post-perk execution before the final cap. Both evaluations include the same independent witness penalty, so witnesses cannot create relief; saturated witness pressure removes it. The direct-watched hard block is not converted into a bonus. AwarenessPoints scales relief automatically. No fixed 65/95-point addition, 80 percent floor, sleep weight/value/occupation multiplier or skill-negation of awareness.

Default AwarenessPoints50 and awake-equivalent attention60 without witnesses: removed loss30, therefore +30 actual execution points before clamping. Attention20 gives+10; attention0 gives0; AwarenessPoints0 gives0. Live diagnostics include awareness-loss, sleep-relief, post-perk hand, cap and final chance.

Retain runtime-aware sleeping recognition, sleeping witness/observation suppression, serialization and existing Papyrus. Automated regressions check exact relief, configuration scaling, witness independence/saturation, awake behavior unchanged and bounds across skill/weight/value. Gameplay confirmation pending.

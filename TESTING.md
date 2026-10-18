# Runtime acceptance gates before 1.0

- Check startup log: two chance call sites installed, matching SKSE runtime; MCM native status ready. Original callers are discovered from the running executable on disk. Redirected live CALL destinations are accepted only if the surrounding instruction context matches. Test with the installed chance-hook mods: both verified caller messages and the successful two-hook message must appear; then compare displayed and actual chance. Unknown instruction changes must disable the overhaul.
- Compare ordinary menu percent to actual repeated theft outcomes for weight/value, skill 0/15/50/100, fortified skill, all relevant pickpocket perks, both standing and sneaking, reverse pickpocket and equipped items. No automatic chance equal to cap; no random draws during preview.
- Adjust awareness/distraction/weight/value/profile point strengths and absolute maximum; reopen selected items; verify cap/display/roll agree. Compare elderly and guard profiles. Confirm skill cannot bypass direct undistracted observation. Open the MCM and change every slider; getters must only read cached/native settings and distance API changes must run from queued game tasks.
- With Standing Pickpocket API v1, confirm its distance slider disables and this MCM controls reach; check hotkey, vanilla activation, followers and horse inventory remain intact. Without it, chance distance enforcement still works; menu-opening reach needs separate acceptance.
- Test native bump tags and animation mods; two distractions then suspicion on repeated contact, no farming by holding contact. Validate seated eating/drinking, conversation facing/rear, crowds, guard/alerted witnesses and follower/guild/outlaw exceptions.
- Successful theft records only actual stolen instance. Save/load before recognition and expiry, repeated same-form stacks, enchantments, mixed legal/stolen stacks, follower gifts, player chests, dropped items, vendor/fence sales and unique-ID changes. Unrelated ownership must stay untouched.
- Equip tracked possessions in front of victim before/after memory deadline. One theft report only, no involuntary search. Hidden/stored/follower-held items are not visually recognized.
- Validate all clocks during menu pause, timescale changes, waiting, cell travel, older and branched saves. Profile scans/cosave size in crowds and a long game. Check unload and deleted-reference cleanup.

Automated math and file checks are not proof that these engine integrations work in-game.

## 0.1.3 acceptance

- Walk/run into an awake NPC and inspect the native small-bump log line. Confirm four-second distraction expiry, no repeated credit while standing against them, and suspicion on the third bump. Test NPC-to-NPC collisions, companions, crowded contacts, cell changes, save/load and pause. The new state is transient and the saved attention/stolen schema is unchanged.
- Hear an ordinary greeting outside dialogue and compare attention/chance before, during and after speech. No greeting bonus inside Dialogue Menu and no automatic direct-watch bypass.
- At skill 15, compare a small gold stack on sleeping/awake NPCs, then increase item weight. Check sleeping victim versus awake witnesses and ensure actual roll/menu share the result.

## 0.1.4 sleep diagnostics

Inspect a sleeping NPC and compare weightless gold, 1-weight items, and heavier items at skill 0/15/100. The Chance log must show sleeping true (sleep-state 7), selected item weight/value, witness penalty, cap and final chance. If sleeping is false, collect the log before assuming balance is the cause. Verify fully awake target loses sleep assistance and witnesses retain full influence. Diagnostics are limited to one changed selection/state/result per second.

## 0.1.5 runtime state access

On 1.6.629+ including installed 1.7.104, inspect a sleeping NPC: diagnostic actor-state-offset must be C0, sleep-state 7, sleeping true. Awake targets must show sleeping false and seated targets must get only sitting treatment. SE 1.5.97 uses B8 and needs a separate live check. Chance and bump sleep exclusion use the same runtime-aware accessor. Existing math balance and saved state are unchanged.

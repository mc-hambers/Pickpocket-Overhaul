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

## 0.1.7 supersedes earlier sleeping-balance checks

No 80% baseline, extra sleep handling, doubled sleeping weight penalty or quarter value/occupation penalty remains. Verify sleeping personal attention is zero, witness attention persists, and the same item/skill rules apply to an awake target with zero awareness. Confirm waking restores facing/detection penalties. Runtime-aware state access and diagnostics remain.

## 0.1.8 derived sleep relief

Verify awake execution is unchanged. Sleeping diagnostics must show relief equal to the awake-equivalent awareness loss minus sleeping awareness loss, including identical witness pressure in both. Test target attention, AwarenessPoints0/25/50, heavy items, saturated witnesses, wake transitions and perk/distraction interactions. No fixed sleep bonus or general awake raw-chance penalty applies. The actual theft and menu use the same calculation.

## 0.1.9 witness and age

With a valid awake witness seeing both player and target, compare Witness strength0/1/3 on the same low-skill item. Log must show raw contribution, configured strength and witness-loss; final chance should fall unless already clamped at0 or cap. Test exclusions and observer facing/sitting/focus. Inspect an ElderRace actor and an ordinary-race actor using a vanilla elderly voice; elderly true and age-relief must appear, with relief responding to Age/occupation points0/7.5. Existing customized INI choices remain unchanged; MCM reset default is7.5.

## 0.1.10 bump reaction acceptance

Compare the SAME item on the SAME target before contact, within six unpaused seconds of an accepted bump, after expiry, and after the third bump. Logs must show bump-active true, positive remaining time and misdirection24 during a valid window. Test no new witnesses versus witnesses reacting to contact, prior suspicion, menu pauses, and unsuccessful native attribution in crowded contacts. Target turning alone must not outweigh the valid distraction; third-bump suspicion intentionally can reduce chance.

## 0.1.11 skill/weather/time

Compare skill0/15/50/100 for coin purses, rings and heavy weapons, same target/environment. Cap should rise more for small items and never exceed maximum or bypass direct observation. Compare before/after relevant perks; execution baseline itself is skill-independent. Test fortified skill, sleep/witness/age and misdirection interactions. Force rain/storm/clear and weather transitions outdoors versus interiors, guards versus civilians. Compare00:00/06:00/12:00/18:00: guards must have time-attention0 as victims and witnesses. Logs must report the new terms. Automated calculations are not live engine acceptance.

## 0.1.12 acceptance

Compare the same gold stack before and after one small bump, including crowded contacts. Check bump observation acceptance and inventory interaction observed flags in the log. Hidden rear inspection, sleeping victims, and active bump windows must not add interaction suspicion. Visible repeated attempts still do. Verify third-bump suspicion and no NPC-to-NPC contact credit. Native contact attribution remains heuristic; live validation is pending.

## 0.1.13 acceptance

- Stand in front of an awake NPC, then move behind: compare lingering-reaction and displayed chance immediately and after several seconds. Repeated menu queries and paused menus must not advance the fade. Save/reload and cell travel must retain the trail correctly.
- Compare identical gold before/after a valid small bump. Verify the fade is bypassed during the six-second window, resumes normally afterward, and no bonus is granted on a third suspicious bump. Test absolute maximum 98, 99 and 100 with sufficient execution chance: ceilings are 99, 100 and 100 respectively. Witness and prior suspicion penalties remain.
- Startup must log the native theft-result hook. Force an actual failed theft and verify its failure log, cancelled bump and 80 percent penalty for 60 unpaused seconds. Successful theft, browsing/closing inventory and unrelated combat must not create a failure. Test reverse pickpocket, older saves, redirected CALL compatibility and save/load during the penalty.
- File audit confirms one native AttemptPickpocket CALL on installed runtime 1.7.104.0. Other SE/AE call sites and live behavior remain untested. Automated checks do not establish in-game results.

## 0.1.14 acceptance

- Repeat the reported large bump on NPC 00039FB7. Confirm type 1 accepted and a large-bump log with a 10-second window and 36 distraction points; compare a small bump's 6 seconds and 24 points.
- Test both sizes in combat, crowds, rain, standing/sneaking and around unrelated NPC collisions. Native size plus recent player motion is required; attribution remains heuristic. Both retain +1 maximum point, slope bypass and failed-theft/witness penalties.
- Test small-to-large upgrades, duplicate callbacks and three separate bumps: an upgrade counts once, duplicates cannot extend indefinitely, and the third separate bump is suspicious for either size.
- Save/load during a large-bump window, and load an older 0.1.13 trail record. Current records retain size; older records default to small. Live gameplay verification remains pending.

## 0.1.15 acceptance

- Startup must log the verified native failed-pickpocket recognition alarm. On installed runtime 1.7.104.0 the alarm is RVA 6827E0 and the native attempt caller is RVA 759DDB.
- Steal an identifiable wearable, wait for the owner to notice, then wear it in their sight. Confirm an appropriate vanilla theft/pickpocket reaction and compare the bounty with a failed vanilla pickpocket in the same crime faction. Test default and custom faction pickpocket fines, crime overhauls, no-crime factions and different voice types. Native behavior may suppress dialogue; no forced mismatched voice fallback exists.
- Wear multiple tracked possessions from the same owner: one scan must generate one alarm, not one fine per item. Save/reload and revisit: reported items must not trigger again. Check expiry, sleeping/absent owners, follower/storage transfers and legal same-form possessions.
- Compare low/high-value worn items: recognition uses the native failed-pickpocket crime rather than independently calculating a value-based theft fine. Actual failed attempts retain their existing native crime handling and must not receive an extra mod-generated fine.
- Automatic binary and package checks are not proof of audible dialogue or runtime bounty behavior.

## 0.1.16 acceptance

- Let NPC B watch the player bump NPC A, then bump B. Check B's bump-witness log and its reduced effect/window. Compare an otherwise similar NPC who did not see the collision. Verify asleep, different-cell, obscured, out-of-range and turned-away actors receive no witness record.
- Repeat distinct contacts with different victims: each seen collision adds 25% wariness up to 75%; one step fades per 30 unpaused seconds. Paused menus must freeze that clock. Small-to-large upgrades and duplicate native signals count once. Victims do not observe their own contact.
- Confirm partial attention suppression, shorter windows and weaker misdirection in normal UI and actual rolls. A wary NPC facing/detecting the player can still block an insufficiently distracting bump. The +1 ceiling exception and failed-theft penalties remain intact.
- Save/reload with wariness and an active large bump; check both persist. Load prior v1/v2 trail data: new wariness defaults to zero. Test crowded contacts and repeated-target suspicion separately. Automated core checks do not establish live visibility or perception behavior.

## 0.1.18 skill ceiling balance

- Revisit the reported items at skills 15-17, then 50 and 100. Inspect skill-starting-ceiling, final cap and post-perk execution. Low skill lowers the starting ceiling; no inexperience-loss is subtracted from execution. UI and actual rolls must share the same calculation.
- Keep skill fixed and compare awareness, witnesses, weight/value, occupation, weather and bump/distraction changes: all must continue to modify the final ceiling. It must not become a fixed skill-only percentage. The valid bump +1 maximum exception stays bounded at 100.
- Confirm perks still improve execution below the cap, while master-skill handling and direct undistracted observation retain prior behavior. Existing bump wariness, failure penalties and save/load acceptance cases still apply.
- Numerical log replays are regression fixtures, not runtime proof of a fixed result under changed conditions.

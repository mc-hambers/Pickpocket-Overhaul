# Pickpocket Overhaul

SKSE awareness-based pickpocketing for Skyrim SE/AE. **0.1.18 development preview**, not the finished 1.0 release. Gameplay validation is pending.

## Chance and situational ceiling (0.1.12)

The situational ceiling now includes bounded skill relief, strongest on light items. It is not a minimum success chance. Starting balance strengths in points:

- Awareness: 50 penalty points (includes target attention, detection, facing, proximity, suspicion and witnesses).
- Distractions: up to 20 relief points.
- Weight: up to 15 penalty points, logarithmically normalized at 30 weight units.
- Value/security: up to 10 penalty points, normalized at 2,000 gold.
- Age/occupation: up to 7.5 points harder or easier. Guards are harder; recognized ElderRace/ElderRaceVampire or the four vanilla elderly voice types reduce age difficulty. Unknown profiles remain neutral. No guessed numerical NPC ages.

These are independent strengths, not percentages of the displayed chance. The absolute maximum remains adjustable, default 98 (a valid bump temporarily permits one extra point, at most 100); there is no 50% floor. An awake NPC facing and detecting the player blocks theft unless a strong distraction or diverted focus breaks that observation. Skill and perks cannot bypass this gate.

Actual execution starts from 80 points minus ordinary weight/security handling losses. Vanilla pickpocket perk/item entry points modify that execution. Skill primarily raises the cap, most for small items, and still improves use of distractions. The final integer is clamped to the situational ceiling. Both native percentage/roll call sites still require successful installation. The ordinary UI text is retained.

## MCM crash report and hardening

The author reported a crash opening the MCM. The full crash log is unavailable, so its cause is unconfirmed. The page no longer queries the player actor value from the Papyrus VM: it reads a cached last-inspected-item ceiling. Setting changes queue Standing Pickpocket distance operations onto the game task queue. Readiness/ownership flags are atomic, option-array access is guarded, and changing settings invalidates the cached readout.

The installed log showed a redirected menu chance CALL, which the live-only scan failed to discover; it disabled both hooks and left vanilla's 90% display. Version 0.1.12 discovers the original callers from the running executable on disk and verifies both live instruction contexts before installing. An executable redirected CALL is supported; changed surrounding instructions or a different caller count are rejected. In-game confirmation is still pending.

For eligible player theft, Pickpocket Overhaul owns both chance calculations, superseding other formulas attached to these two CALLs. Inactive and non-player calls retain their previous per-site delegates. Other mods replacing the chance formula require compatibility testing; no companion files are changed.

## Actor-state correction (0.1.12)

Sleep, sitting and bump eligibility now read ActorState through CommonLib's runtime-aware AsActorState accessor. The combined SE/AE build's inherited layout uses offset B8, while Skyrim 1.6.629 and newer uses C0; direct inherited reads could therefore report a sleeping NPC as state 0. Chance diagnostics now include the selected actor-state offset. This repairs recognition without another balance change; live confirmation remains pending.

## Included implementation

- Native chance hooks at both verified original calls to Skyrim's shared ComputePickpocketSuccess function. Unknown call-site counts disable hook installation. No global fPickPocketMaxChance edits.
- SkyUI MCM: situational factor strengths, absolute maximum, reach multiplier, weight/value penalties, attention/witness strength, stolen-tag and recognition timer scales. Settings persist to the INI.
- Optional Standing Pickpocket API v1 integration owns its distance override and lets that mod disable its distance slider. Older versions without the API cannot have their slider disabled. No companion files are modified.
- Attention uses facing, close proximity, detection, observed sneaking, repeat menu interactions, alarm/combat after a closed unsuccessful interaction, nearby guards/alerts, sleeping, sitting, scenes and headtracking another actor.
- Crowds and recent combat/hit commotion help misdirection. Native IsSmallBump condition plus high-process bump state/timestamp changes detect light engine reactions; recent nearby player movement supplies conservative attribution. First two bumps grant six seconds of distraction (24 raw misdirection, 80% of the default distraction allowance); further bumps within 60 seconds cancel the window and add suspicion. Duplicate events within one second are ignored. No artificial bump dialogue is played. Eating/drinking still uses animation-event adapters.
- NPC greetings to the player outside the Dialogue Menu reduce attention by 8 points while native IsTalking and IsGreetingPlayer are true. Facing/detected observation can still block theft.
- Sleeping suppresses the victim's attention: facing, proximity, detection, suspicion, observed sneaking, repeated interactions and nearby alerts do not contribute personal awareness while asleep. Sleeping bystanders cannot witness theft, observe sneaking or visually recognize worn possessions. Awake witnesses retain their own penalties. No fixed sleeping bonus or special weight/value/occupation multiplier applies; normal skill, perks and item difficulty remain.
- Followers do not witness; guild actors require a friendly victim relationship; mapped bandits, beggars and thieves require a shared mapped faction. Other witnesses require visibility to player and victim.
- Successful menu transfers can record uniquely identifiable stolen item instances. Saved victim, ownership, instance ID and deadlines follow unique-ID changes and identified container transfers. Recognition and stolen status age separately with item value.
- A victim noticing a loss becomes more suspicious. A living visible victim can report a recognized tracked item equipped by the player once. Giving it to a follower or storing it avoids that equipped-item check. No forced player search or inventory confiscation.
- Expired tracked ownership is reassigned to the player's base without deleting items or changing unrelated ownership. It is serviced when the tracked container/item can be resolved.

## Limitations before 1.0

- No in-game validation yet. Native perk argument order and two chance call sites were inspected in installed 1.7.104; 1.5.97/1.6.x still need equivalent live checks.
- Native bumps are sampled every 100 ms on the game task queue. The state does not expose an instigator, so contact attribution uses nearby player movement and rejects a closer competing actor. This can miss ambiguous crowded bumps; it is not a Havok contact listener. Eating/drinking animation tags still require validation.
- Arguments, explosions without hit events, and fleeing-specific signals are not yet separately recognized; scenes, combat and hits currently supply the available commotion signals.
- A transfer that cannot identify exactly one NPC-owned inventory instance is left unchanged. Merged stacks and ownership by factions instead of the victim need broader tracking before complete dynamic stolen-tag coverage can be claimed.
- Theft recognition currently covers tracked equipped possessions, not magical inspection of every hidden item. Recognition invokes the native failed-pickpocket reaction and its vanilla dialogue; no custom confrontation dialogue or searching AI package is included.
- Faction mappings cover vanilla guilds, bandits, thieves and beggars. Additional outlaw factions need mapping.
- Other mods replacing these chance call sites or the chance function need explicit testing. Artful Dodger is a reference, not a dependency; its independent global cap controller is unnecessary.

## Requirements and installation

Matching SKSE and Address Library, Microsoft VC++ x64 runtime, SkyUI for MCM. Enable the ESL-flagged PickpocketOverhaul.esp and install the DLL, two compiled Papyrus scripts, INI and SEQ. Standing Pickpocket is optional. Dynamic Pickpocket Loot can remain separate; this mod does not generate pocket loot or restore equipment.

**Papyrus is used for the SkyUI MCM and its start-enabled quest only.** Chance, attention, observations, ownership tracking and save state live in C++/SKSE.

## Build and source

Complete source and dependency source are in Pickpocket-Overhaul-source.zip. Native files are also browsable in src/. The matching CommonLib static library is supplied separately as CommonLibSSE.lib.xz, with Restore-CommonLib.py. Build instructions are in BUILD.md. No Artful Dodger files, game masters, compiler binaries or build caches are included.

## License

Copyright (C) 2026 Maxmc. Original mod code is GPL-3.0-or-later. Distributed WITHOUT ANY WARRANTY, including merchantability or fitness for a particular purpose. See LICENSE.txt. Third-party source retains its licenses and exceptions.

The former 80% sleeping baseline is removed. Sleep relief comes from the removed attention penalty; it does not establish a minimum success chance.

Sleeping relief is calculated from the attention penalty sleep actually removes: the awake-equivalent awareness loss minus the sleeping awareness loss, with awake witnesses included in both. That difference helps actual execution below the cap, so the sleep effect is visible even for low skill. There is no fixed sleeping bonus, minimum chance, general awake execution penalty, or sleeping item multiplier. The final situational cap still applies.

Witness pressure now subtracts from calculated execution as well as limiting its ceiling, so Witness strength affects low-skill chances below the cap. Witness contributions use their current facing, detection, sitting, scenes, focus and suspicion. Elderly relief likewise affects execution beneath the cap; occupation handling is unchanged. Age/occupation defaults to 7.5 points in native settings, INI and MCM reset. Existing customized INI values are retained. Race, voice, elderly status, age relief and witness strength/loss are logged. Appearance alone cannot establish chronological age; unsupported records remain neutral.

During a recognized first/second light-bump window, immediate facing, proximity and detection reactions do not add target attention. Existing suspicion/failure/menu-interaction pressure and independent witnesses still count. The target is not newly marked as having observed sneaking during that window. Six seconds of active play are allowed; duplicate signals and third-or-later suspicion rules remain. Logs show active/remaining bump window, count, misdirection, facing/detected flags and suspicion.

## Skill, weather and time (0.1.12)

Skill contributes bounded cap relief: normalized skill times lightness times a precision/security allowance, capped at20 points before lightness. Lightness falls logarithmically to zero at weight30. At default settings the allowance is12 points plus partial relief from value (80%) and weight (50%) penalties, still bounded by the20-point limit. Awareness, witnesses, age/occupation, item difficulty and the MCM absolute maximum still apply. Direct undistracted observation remains a hard block. Skill no longer directly ramps execution from5 to100; neutral execution is80 minus20 times normalized weight and12 times normalized value before perks. Perks therefore decide how close execution gets to the skill-adjusted cap. There is no guaranteed chance equal to the cap.

Outdoor rainy weather adds6 raw distraction; a rainy weather record with thunder sound/lightning adds12. Current/previous weather blend with the sky transition. Rain does not add distraction inside interior cells; guards receive outdoor weather effects normally.

Time of day changes awake personal attention smoothly: up to+5 at noon, down to-10 at midnight, neutral at06:00/18:00. Guards are entirely exempt from this clock-based adjustment, including when witnessing theft. Other awareness factors continue to apply to guards. Sleeping still zeroes personal attention. Logs include hour, time attention, guard status, weather distraction and skill-cap relief. These are initial balance values pending gameplay verification.

## 0.1.13 attention fade and failed thefts

Personal facing/proximity/detection attention rises immediately and fades at 5 points per second of unpaused gameplay. Moving behind an NPC no longer removes that attention immediately. A valid small-bump window bypasses this fading component, without removing witnesses, existing suspicion or failed-theft penalties. Sleep clears personal reaction memory. Memory is saved in a separate compatible SKSE record; older saves start with no trail.

During a valid bump, the absolute maximum and execution allowance each gain one percentage point: 98 can reach 99, 99 can reach 100, and 100 remains 100. This remains a ceiling; other factors can still lower the result. Third and later suspicious bumps do not qualify.

A failed native theft attempt cancels the current bump window, sets at least 40 suspicion and reduces both execution and its ceiling by 80% for 60 seconds of unpaused gameplay. Another failed attempt renews the timer. Ordinary inventory closing, unrelated combat and successful theft do not count as failed attempts. The native attempt-result CALL is independently validated and logged; if its instruction context is unsupported, no failure-result hook is installed and an error explains the limitation. Existing chance hooks remain independently guarded.

All new gameplay handling is SKSE/C++; the existing Papyrus SkyUI MCM remains unchanged. Runtime validation of these new paths is pending.

## 0.1.14 small and large bumps

Both native bump sizes qualify when attributed to recent close player motion. Small bumps provide 24 distraction points for 6 seconds; large bumps provide 36 points for 10 seconds. The shared distraction normalization can saturate in storms/crowds, so the larger bump does not promise a higher displayed percentage when already capped. Both bypass fading personal attention and permit the same one-point absolute-ceiling exception (never above 100).

A small-to-large reaction within the one-second duplicate guard upgrades the same collision without adding a second repetition. The third separate bump within 60 seconds still cancels the benefit and adds suspicion, regardless of size. Combat no longer excludes native contact from attribution; native hostility, crime consequences, witnesses and failed-theft penalties are preserved.

Bump size is saved in version 2 of the separate attention-trail record. Existing version 1 trails load as small bumps; the original attention/stolen record is unchanged. No new Papyrus scripts or MCM changes.

## 0.1.15 recognized-item crime and dialogue

An owner recognizing their tracked stolen possession equipped by the player now invokes Skyrim's native failed-pickpocket alarm rather than the generic value-based theft alarm. That engine path selects the vanilla pickpocket/thief response and applies the native pickpocket crime/bounty settings, including crime-mod changes through the same path. The mod adds no separate fine and does not force a specific line from an incompatible voice. NPC voice/dialogue availability, faction crime rules and native alarm behavior still apply; live audio and bounty outcomes require testing.

Recognition remains limited to visible, equipped, tracked possessions inside the owner's recognition window. Each item is reported once; several worn items recognized from one victim in the same scan cause one confrontation/alarm. Saved recognition flags are retained on update, so previously reported items are not reported again. Hidden possessions, stored items and follower-held items remain outside this visual check.

The alarm target and the caller inside the native theft-attempt routine are validated against the runtime executable and live instruction context. Unsupported mappings disable recognition reporting with an error, rather than silently charging a generic theft fine. No additional Papyrus, voice files or copied vanilla dialogue records are included.

## 0.1.16 watching other bumps

An awake NPC who sees the player bump another NPC becomes less susceptible to the same trick. The observer must share the cell, be within 900 units of the victim, see/detect the player, have line of sight to the victim and face the contact within a 90-degree heading angle. Mere proximity or hearing a collision does not qualify. This personal wariness is separate from crime-witness exemptions; no crime is reported for observing a bump.

Each distinct observed collision reduces future bump effectiveness by 25%, up to a 75% reduction. Wariness fades continuously at one step per 30 seconds of unpaused play. The effectiveness is sampled when that NPC is bumped, reducing distraction strength, duration and the amount of personal attention suppressed. Small/large baseline strengths remain 24/36 points and 6/10 seconds. The separate +1 maximum exception stays unchanged; witnesses, hard observation and failed-theft penalties can still lower or block a theft.

Small-to-large upgrades and duplicate callbacks do not add another witnessed collision. The victim does not count their own collision as watching another person. Wariness and active-bump effectiveness persist in attention-trail record version 3; versions 1 and 2 load with no new wariness. No additional Papyrus or MCM changes. Live perception/visibility testing remains pending.

## 0.1.18 skill-based situational starting ceiling

The 0.1.17 inexperience subtraction has been removed. Perk-modified execution is restored unchanged. Skill now contributes more strongly to the situational ceiling: its starting value is `AbsoluteMaximum * (1 - 0.4*(1 - skill/100)^1.5)`, with skill clamped to 0..100. At the default maximum 98 this starts around 59 at skill 0, 67 at skill 15, 84 at skill 50, and 98 at skill 100.

This is the starting ceiling, not the final chance or a separate hard skill cap. Awareness, witnesses, weight, value and occupation adjust it, while distractions and the existing bounded light-item skill relief can raise it toward the absolute maximum. The valid-bump +1 exception is preserved. Perks still determine execution beneath that situational ceiling, and direct undistracted observation still blocks theft. No flat inexperience loss, guaranteed chance, MCM additions, Papyrus changes or saved-state migration.

Reconstructed log examples yield approximately 40% for the skill-17/value-120 case and 26% for the skill-16/value-160 case. These are numerical regression examples, not live gameplay measurements. Bump sizes, observed-bump wariness, attention fading, failed-theft penalties and native recognition dialogue/bounty handling remain.

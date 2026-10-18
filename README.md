# Pickpocket Overhaul

SKSE awareness-based pickpocketing for Skyrim SE/AE. **0.1.4 development preview**, not the finished 1.0 release. Gameplay validation is pending.

## Chance and situational ceiling (0.1.4)

The old skill-scaled minimum/maximum cap is removed. Skill no longer sets the ceiling. Starting balance strengths in points:

- Awareness: 50 penalty points (includes target attention, detection, facing, proximity, suspicion and witnesses).
- Distractions: up to 20 relief points.
- Weight: up to 15 penalty points, logarithmically normalized at 30 weight units.
- Value/security: up to 10 penalty points, normalized at 2,000 gold.
- Age/occupation: up to 5 points harder or easier. Guards are harder; recognized ElderRace/ElderRaceVampire reduce profile difficulty. Unknown profiles remain neutral. No guessed numerical NPC ages.

These are independent strengths, not percentages of the displayed chance. The absolute maximum remains adjustable, default 98; there is no 50% floor. An awake NPC facing and detecting the player blocks theft unless a strong distraction or diverted focus breaks that observation. Skill and perks cannot bypass this gate.

Actual chance comes from skill, reduced handling/security losses as skill improves, vanilla pickpocket perk/item entry points, and skill-dependent use of distraction. The final integer is clamped to the situational ceiling. Both native percentage/roll call sites still require successful installation. The ordinary UI text is retained.

## MCM crash report and hardening

The author reported a crash opening the MCM. The full crash log is unavailable, so its cause is unconfirmed. The page no longer queries the player actor value from the Papyrus VM: it reads a cached last-inspected-item ceiling. Setting changes queue Standing Pickpocket distance operations onto the game task queue. Readiness/ownership flags are atomic, option-array access is guarded, and changing settings invalidates the cached readout.

The installed log showed a redirected menu chance CALL, which the live-only scan failed to discover; it disabled both hooks and left vanilla's 90% display. Version 0.1.4 discovers the original callers from the running executable on disk and verifies both live instruction contexts before installing. An executable redirected CALL is supported; changed surrounding instructions or a different caller count are rejected. In-game confirmation is still pending.

For eligible player theft, Pickpocket Overhaul owns both chance calculations, superseding other formulas attached to these two CALLs. Inactive and non-player calls retain their previous per-site delegates. Other mods replacing the chance formula require compatibility testing; no companion files are changed.

## Included implementation

- Native chance hooks at both verified original calls to Skyrim's shared ComputePickpocketSuccess function. Unknown call-site counts disable hook installation. No global fPickPocketMaxChance edits.
- SkyUI MCM: situational factor strengths, absolute maximum, reach multiplier, weight/value penalties, attention/witness strength, stolen-tag and recognition timer scales. Settings persist to the INI.
- Optional Standing Pickpocket API v1 integration owns its distance override and lets that mod disable its distance slider. Older versions without the API cannot have their slider disabled. No companion files are modified.
- Attention uses facing, close proximity, detection, observed sneaking, repeat menu interactions, alarm/combat after a closed unsuccessful interaction, nearby guards/alerts, sleeping, sitting, scenes and headtracking another actor.
- Crowds and recent combat/hit commotion help misdirection. Native IsSmallBump condition plus high-process bump state/timestamp changes detect light engine reactions; recent nearby player movement supplies conservative attribution. First two bumps grant four seconds of distraction (24 raw misdirection, 80% of the default distraction allowance); further bumps within 60 seconds cancel the window and add suspicion. Duplicate events within one second are ignored. No artificial bump dialogue is played. Eating/drinking still uses animation-event adapters.
- NPC greetings to the player outside the Dialogue Menu reduce attention by 8 points while native IsTalking and IsGreetingPlayer are true. Facing/detected observation can still block theft.
- Sleeping zeros target attention and gives up to 95 handling points for light items, decreasing with squared normalized weight. Sleeping doubles the weight penalty on the cap and adds a weight handling loss; witnesses and the absolute maximum still apply; value and occupation cap penalties are quarter strength during sleep.
- Followers do not witness; guild actors require a friendly victim relationship; mapped bandits, beggars and thieves require a shared mapped faction. Other witnesses require visibility to player and victim.
- Successful menu transfers can record uniquely identifiable stolen item instances. Saved victim, ownership, instance ID and deadlines follow unique-ID changes and identified container transfers. Recognition and stolen status age separately with item value.
- A victim noticing a loss becomes more suspicious. A living visible victim can report a recognized tracked item equipped by the player once. Giving it to a follower or storing it avoids that equipped-item check. No forced player search or inventory confiscation.
- Expired tracked ownership is reassigned to the player's base without deleting items or changing unrelated ownership. It is serviced when the tracked container/item can be resolved.

## Limitations before 1.0

- No in-game validation yet. Native perk argument order and two chance call sites were inspected in installed 1.7.104; 1.5.97/1.6.x still need equivalent live checks.
- Native bumps are sampled every 100 ms on the game task queue. The state does not expose an instigator, so contact attribution uses nearby player movement and rejects a closer competing actor. This can miss ambiguous crowded bumps; it is not a Havok contact listener. Eating/drinking animation tags still require validation.
- Arguments, explosions without hit events, and fleeing-specific signals are not yet separately recognized; scenes, combat and hits currently supply the available commotion signals.
- A transfer that cannot identify exactly one NPC-owned inventory instance is left unchanged. Merged stacks and ownership by factions instead of the victim need broader tracking before complete dynamic stolen-tag coverage can be claimed.
- Theft recognition currently covers tracked equipped possessions, not magical inspection of every hidden item. NPC pursuit/dialogue or a custom searching AI package is not included.
- Faction mappings cover vanilla guilds, bandits, thieves and beggars. Additional outlaw factions need mapping.
- Other mods replacing these chance call sites or the chance function need explicit testing. Artful Dodger is a reference, not a dependency; its independent global cap controller is unnecessary.

## Requirements and installation

Matching SKSE and Address Library, Microsoft VC++ x64 runtime, SkyUI for MCM. Enable the ESL-flagged PickpocketOverhaul.esp and install the DLL, two compiled Papyrus scripts, INI and SEQ. Standing Pickpocket is optional. Dynamic Pickpocket Loot can remain separate; this mod does not generate pocket loot or restore equipment.

**Papyrus is used for the SkyUI MCM and its start-enabled quest only.** Chance, attention, observations, ownership tracking and save state live in C++/SKSE.

## Build and source

Complete source and dependency source are in Pickpocket-Overhaul-source.zip. Native files are also browsable in src/. The matching CommonLib static library is supplied separately as CommonLibSSE.lib.xz, with Restore-CommonLib.py. Build instructions are in BUILD.md. No Artful Dodger files, game masters, compiler binaries or build caches are included.

## License

Copyright (C) 2026 Maxmc. Original mod code is GPL-3.0-or-later. Distributed WITHOUT ANY WARRANTY, including merchantability or fitness for a particular purpose. See LICENSE.txt. Third-party source retains its licenses and exceptions.

# Pickpocket Overhaul

SKSE awareness-based pickpocketing for Skyrim SE/AE. **0.1.1 development preview**, not the finished 1.0 release. Gameplay validation is pending.

## Chance and situational ceiling (0.1.1)

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

The installed 0.1.0 log also shows only one of two expected chance calls, causing hooks to be disabled. This is a separate unresolved runtime conflict, not proof of the MCM crash cause. 0.1.1 logs the remaining caller and audited site bytes on 1.7.104 without overwriting an unknown hook. It must not claim the overhaul is active when display/roll coverage is incomplete.

## Included implementation

- Native chance hooks at both direct calls to Skyrim's shared ComputePickpocketSuccess function. Unknown call-site counts disable hook installation. No global fPickPocketMaxChance edits.
- SkyUI MCM: situational factor strengths, absolute maximum, reach multiplier, weight/value penalties, attention/witness strength, stolen-tag and recognition timer scales. Settings persist to the INI.
- Optional Standing Pickpocket API v1 integration owns its distance override and lets that mod disable its distance slider. Older versions without the API cannot have their slider disabled. No companion files are modified.
- Attention uses facing, close proximity, detection, observed sneaking, repeat menu interactions, alarm/combat after a closed unsuccessful interaction, nearby guards/alerts, sleeping, sitting, scenes and headtracking another actor.
- Crowds and recent combat/hit commotion help misdirection. Animation-event adapters handle recognized bump/eat/drink events; repeated bumps increase suspicion.
- Followers do not witness; guild actors require a friendly victim relationship; mapped bandits, beggars and thieves require a shared mapped faction. Other witnesses require visibility to player and victim.
- Successful menu transfers can record uniquely identifiable stolen item instances. Saved victim, ownership, instance ID and deadlines follow unique-ID changes and identified container transfers. Recognition and stolen status age separately with item value.
- A victim noticing a loss becomes more suspicious. A living visible victim can report a recognized tracked item equipped by the player once. Giving it to a follower or storing it avoids that equipped-item check. No forced player search or inventory confiscation.
- Expired tracked ownership is reassigned to the player's base without deleting items or changing unrelated ownership. It is serviced when the tracked container/item can be resolved.

## Limitations before 1.0

- No in-game validation yet. Native perk argument order and two chance call sites were inspected in installed 1.7.104; 1.5.97/1.6.x still need equivalent live checks.
- Native animation tags vary by behavior pack; physical bump attribution and eating/drinking coverage require validation. The adapters do not claim a general physics collision sensor.
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

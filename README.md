# Pickpocket Chance Rework 1.1.0

[Download 1.1.0](https://github.com/mc-hambers/Pickpocket-Overhaul/releases/tag/source-1.1.0) | [Complete source](Pickpocket-Overhaul-source.zip) | [Matching precompiled CommonLib](CommonLibSSE.lib.xz)

Restore the precompiled dependency with `python Restore-CommonLib.py <extracted-source-root>`, or rebuild it from the included source.

QuickPocket compatibility is now built into PickpocketOverhaul.dll. No separate
PCRQuickPocket.dll is needed. Existing runtime filenames, settings and co-save
identities remain unchanged.

## New in 1.1.0

- Automatically detect the supported QuickPocket 1.0.0 binary and connect its
  percentage previews and actual attempts to PCR's existing calculations.
- Use PCR's existing Karma toggle/history, inventory-opening suspicion,
  failed-theft penalties and successful-stack penalties in QuickPocket sessions.
- Preserve QuickPocket's own transfer, planting/perk filtering and crime flow.
- Enable attribution of QuickPocket transfers for PCR's stolen-item tracking.
- Missing or unsupported QuickPocket builds are left untouched. QuickPocket is
  optional; ordinary PCR functionality does not require it.
- Remove the old separate `PCRQuickPocket.dll` and restart Skyrim when upgrading.
  If detected, it takes precedence and built-in hooks stay off to avoid duplicates.
- The Standing QuickPocket adapter remains separate and handles only standing
  hotkey access. Standing Pickpocket remains unchanged at 1.0.2.
- Equipped-item drops are not redirected for QuickPocket's transfer path. Its
  normal percentage text is retained rather than adding PCR debug parentheses.

Native builds, 140,698 core checks, 35 QuickPocket native/routing checks, Papyrus
compilation and packaging checks pass. QuickPocket gameplay remains unverified.

## Retained features

- Repair duplicate SkyUI registrations for this exact MCM quest once, with a
  bounded cleanup and callback guard. Other mods are not reset. MCM script
  revision 110; release is 1.1.0.

- MCM displays Pickpocket Chance Rework, refreshes its old registration once for
  existing saves, and opens a Home page featuring the mod thumbnail. Settings
  and Debug remain separate. The internal MCM revision is 110; release is 1.1.0.

- MCM ownership settings include a Stolen Goods Values section: minimum stolen
  value (20 gold), local valuables (100 gold), and high-value stolen goods
  (over 300 gold). Whole-gold sliders persist to the INI and keep thresholds
  ordered. Local ordinary theft and local valuables both follow tag expiry.

- Dropping and picking up a tracked stolen item preserves its original hold and
  fade timer. Ground-reference identity is captured before queued processing;
  existing history is relocated before considering a new theft. Ground history
  uses the existing co-save format. In-game drop/pickup testing remains pending.

- Fixed modern-AE heap corruption in the first-load stolen-inventory migration:
  new item-data lists reserve at least 32 bytes before the game constructor runs.
  Allocation failure safely defers migration. In-game crash regression testing is pending.

- Better guard confiscation (on): stolen items worth at least 20 gold from the
  arresting hold remain eligible until their normal stolen-tag fade timer ends.
  Stolen items worth more than 300 gold can be confiscated in any hold while
  their tag remains active. Cheaper items are ignored, except contraband.
  Contraband includes lockpicks, skooma, moon sugar, Sleeping Tree Sap, human
  flesh/hearts, black soul gems and blood potions. Quest items are excluded.
  Confiscated items move to the hold's vanilla evidence chest and show localized
  item-removal notifications. Evidence ownership and fine/jail behavior remain vanilla.
  Existing stolen player inventory is imported on the first playable load after
  updating; the current hold becomes its origin. Existing tracked timers are
  retained. A co-save migration marker prevents repeating this import.
  Contraband Confiscation disables only this mod's contraband classification.
- Dynamic tags (on): tracked goods retain their origin and theft time in the SKSE
  co-save. While carried, tags can become inconspicuous outside their hold and
  return on re-entry while identifiable. Permanent expiry still uses the value
  timer; storage ages tags twice as fast. Ground drops remain NPC-owned.
- Artful Dodger owns the live hard maximum from fPickPocketMaxChance when present,
  including karma and bump rolls. No minimum success floor is applied. Its minimum
  MCM endpoint still determines its skill-based maximum, as its author designed.
- Difficulty point skew: 0-3x, default 1x. Scales difficulty deductions, keeping
  caps, perks and beneficial distraction/age effects independent.
- MCM: two-column gold/cyan section headers, 
  compatibility provider readout, descriptions, existing hover timer panel.

New INIs default to karma/opening/guard/tracking/tags enabled, witness 1x,
maximum 97%, recognition 1 day, tag base 3 days, reach 0.45x, point skew 1x.
Existing INI preferences are preserved. INI is generated only when missing.
Guard thresholds are configurable under [Settings]: MinimumConfiscationGold=20,
HighValueStolenGold=300,
ValuableStolenGold=100, local eligibility follows TagDays (whole in-game days).

Gameplay logic is native SKSE. The MCM and existing recovery path use Papyrus.
No new dialogue/audio or confrontation systems have been introduced.
Build/tests are automated evidence, not in-game proof of confiscation, equipping,
MCM appearance or live UI/roll compatibility. Test these before publishing.
Full matching source is provided as a separate source ZIP.

---

# Pickpocket Chance Rework 1.0.0

An SKSE pickpocket overhaul for Skyrim SE/AE. Development build; in-game validation remains pending.

Pickpocket chances combine awareness, witnesses, distractions, item weight/value, occupation, age, skill and perks. Includes attention decay, bump distractions and wariness, repeated-opening suspicion, per-stolen-stack penalties and optional bad-luck protection. The displayed chance follows the calculated result. Simonrim cap integration and Standing Pickpocket reach integration are built in.

The MCM controls witness strength, maximum chance, reach, stolen-item tracking and tag expiration, recognition duration, equipped-item drops and debug options. Preferences are stored in an automatically generated INI; no INI is shipped. Per-character history is stored in the SKSE co-save.

Tracked stolen tags fade twice as fast in storage containers. Wearing or wielding tracked stolen items can still be recognized within the owner's memory window. This uses the vanilla crime response; there is no custom dialogue or confrontation system.

Equipped-item drops remain WIP and disabled by default. Awake targets can lose their original equipped instance onto the ground. Outside combat they retrieve and equip it after a brief delay, with 270-unit pickup reach and persistent recovery attempts. Combat prioritizes weapons; sleeping thefts retain normal transfers. Ground items retain NPC ownership. Witnessed player theft of these exact dropped items uses the current pickpocket fine as nonviolent crime gold and suppresses the ordinary ground-theft combat alarm. Other ground theft is unchanged. Existing combat is not cancelled.

Removed in 0.1.59: all custom reaction speech, dialogue records/hooks, voice assets/BSA, optional theft alarms/searches/confrontations, their MCM toggle, event script, and event save records. Older co-saves retain ordinary loot, tracking, karma and recovery state; obsolete event records are ignored.

Installation: replace the previous mod installation. Remove the old PickpocketOverhaul.bsa, its Sound/Voice/PickpocketOverhaul.esp directory and PO_TheftResponse.pex if installing manually. Keep your generated INI and saves. Do not delete unrelated files.

The runtime ZIP contains the SKSE DLL, ESL-flagged ESP, two compiled scripts, SEQ and licensing. No custom dialogue, audio archive, Docs directory or bundled configuration.

Build and packaging instructions are in BUILD.md. Source is GPL-3.0-or-later; see LICENSE.txt.

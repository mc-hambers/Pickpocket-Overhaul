# QuickPocket built-in integration (PCR 1.1.0)

Optional QuickPocket 1.0.0 support is inside PickpocketOverhaul.dll. Its numerical
previews and attempts use PCR calculations and Karma/penalty handling. Exact
module fingerprint and hook bytes are verified before changes in process memory.
QuickPocket and QuickLoot IE remain separately installed requirements only when
using QuickPocket. Their installed files are not modified.

Remove the retired PCRQuickPocket.dll and restart. If it remains installed,
built-in hooks are skipped to prevent double hooking; a warning is logged.
StandingQuickPocket.dll is different: keep that optional adapter if you want
Standing Pickpocket's button to open QuickPocket while standing. Hook ranges do
not overlap. No Standing Pickpocket update is required.

QuickPocket retains normal transfers: PCR equipped-item drops are not integrated.
Actual chance display/rolls, Karma persistence, penalties and inventory ownership
still require in-game testing. Missing/unsupported QuickPocket leaves PCR's
normal behavior active.

# Explicit loyalty and argument mappings

Optional file: `Data/SKSE/Plugins/PickpocketOverhaul_Compatibility.ini`.
Create it yourself only when needed. No INI is shipped or overwritten. Restart Skyrim after editing this file; mappings resolve after plugins load. The ordinary MCM settings INI is a separate generated file.

Use exact plugin filename and hexadecimal LOCAL form ID, not the load-order-prefixed runtime ID. Both faction and scene form types are checked. Missing plugins, wrong types, invalid IDs and unknown sections are logged and ignored. No faction-name inference is performed. The file adds mappings; it does not edit records or create faction membership.

Example (replace the example plugin and form IDs with real records):

```ini
[Guild]
1=ExampleGuild.esp|000800
[Outlaw]
1=ExampleOutlaws.esp|000801
[OutlawLoyalty]
1=ExampleOutlaws.esp|000802
[Imperial]
1=ExampleSoldiers.esp|000803
[Stormcloak]
1=ExampleSoldiers.esp|000804
[ArgumentScenes]
1=ExampleScenes.esp|000805
```

- Guild: classify witnesses with friend/allied-victim loyalty rules.
- Outlaw: classify witnesses; personal friendship or a shared OutlawLoyalty faction is required. Do not map broad hostile coalitions as OutlawLoyalty unless they really should protect each other.
- OutlawLoyalty: a specific local group; also classifies its members as outlaws. Both victim and witness must belong to this exact faction, unless personally friendly.
- Imperial / Stormcloak: add members to the corresponding side. Soldiers protect a victim mapped to the same side or a personal friend. Generic faction ally reactions do not automatically join opposite sides.
- ArgumentScenes: exact SCEN forms. Relief applies only while an actor's current scene matches an entry and is playing. Other dialogue is not guessed to be an argument.

Precedence: follower exclusion, Civil War side rules, outlaw rules, guild rules, ordinary witnesses. Sleeping/visibility/distance checks still apply. Faction mapping does not bypass line of sight.

Built-in guilds: Thieves Guild, College of Winterhold, Companions, Dark Brotherhood. Built-in outlaw classification: BanditFaction, BeggarFaction and ThievesGuildFaction, plus explicit local groups. Generic BanditFaction is deliberately excluded from shared loyalty. Built-in local groups: ThievesGuildFaction, Valtheim Keep, Robbers Gorge, Ansilvund, Icerunner, MS07BanditFaction and MS07BanditSiblings. Other camps require explicit mappings or personal friendship. Civil War uses the main Imperial/Stormcloak factions plus their NPC factions.

Built-in arguments, Skyrim.esm local IDs: C06UnderforgeArgument E4CD7; C00KodlakVilkasArgueAboutPlayer A3EB7; C00KodlakVilkasArgueScene A3EA6; MS08ArgueScene 55FB4; MS10AdelaisaAndOrthusArgue 52282; WhiterunCivilWarArgueScene 37CC4; MS12NurelionQuintusArgueScene 2D51A. These were identified by exact records in the installed vanilla master. No runtime name matching is used.

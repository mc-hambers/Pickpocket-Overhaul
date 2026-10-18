# Runtime acceptance gates before 1.0

- Check startup log: two chance call sites installed, matching SKSE runtime; MCM native status ready.
- Compare ordinary menu percent to actual repeated theft outcomes for weight/value, skill 0/15/50/100, fortified skill, all relevant pickpocket perks, both standing and sneaking, reverse pickpocket and equipped items. No automatic chance equal to cap; no random draws during preview.
- Adjust both endpoints and penalties; reopen selected items; verify cap/display/roll agree. Check min/max crossing and 0/100 endpoints.
- With Standing Pickpocket API v1, confirm its distance slider disables and this MCM controls reach; check hotkey, vanilla activation, followers and horse inventory remain intact. Without it, chance distance enforcement still works; menu-opening reach needs separate acceptance.
- Test native bump tags and animation mods; two distractions then suspicion on repeated contact, no farming by holding contact. Validate seated eating/drinking, conversation facing/rear, crowds, guard/alerted witnesses and follower/guild/outlaw exceptions.
- Successful theft records only actual stolen instance. Save/load before recognition and expiry, repeated same-form stacks, enchantments, mixed legal/stolen stacks, follower gifts, player chests, dropped items, vendor/fence sales and unique-ID changes. Unrelated ownership must stay untouched.
- Equip tracked possessions in front of victim before/after memory deadline. One theft report only, no involuntary search. Hidden/stored/follower-held items are not visually recognized.
- Validate all clocks during menu pause, timescale changes, waiting, cell travel, older and branched saves. Profile scans/cosave size in crowds and a long game. Check unload and deleted-reference cleanup.

Automated math and file checks are not proof that these engine integrations work in-game.

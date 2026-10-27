# 0.1.59-dev validation

Automated core behavior tests, native build/exports, Papyrus compilation, INI ownership, recovery contracts, exact plugin records and runtime package exclusions are checked. The retired dialogue, event and voice implementations are absent from the active source and runtime package.

The installed Skyrim 1.7.104.0 executable has nine direct callers of the Actor StealAlarm function (address-library ID 37422, RVA 0x681DE0). Runtime installation verifies all caller contexts before patching. If verification fails, new equipped drops are disabled. Other actors and untracked items forward to the original handler.

In-game checks still required: successful and failed ordinary pickpockets; unseen and witnessed pickup of mod-dropped clothes/weapons; no added combat; crime-faction bounty matching iCrimeGoldPickpocket; no bounty for player-owned gear; unrelated theft unchanged; repeated NPC recovery; no forced cancellation of existing combat; updates from older saves; storage tag aging and MCM persistence.

No automated result proves live gameplay behavior. Replace the old installation to remove the retired BSA, voice folder and event script; preserve generated INI and saves.

## PCR 1.1.0 QuickPocket integration

- [ ] QuickPocket absent: ordinary PCR behavior and clean log.
- [ ] QuickPocket 1.0.0: built-in compatibility installation logged.
- [ ] UI percentage reflects PCR; actual attempts use the same calculations.
- [ ] Karma toggle and history persist across victims and save/load.
- [ ] Open/close, failed attempts and stolen stacks apply existing penalties once.
- [ ] Planting, theft ownership, tag fading and guard confiscation remain correct.
- [ ] Optional StandingQuickPocket adapter also opens/closes correctly.
- [ ] Legacy PCRQuickPocket DLL warns and prevents duplicate built-in hooks.
- [ ] Unsupported QuickPocket fingerprint remains untouched.

These are in-game checks, not fulfilled by native tests.

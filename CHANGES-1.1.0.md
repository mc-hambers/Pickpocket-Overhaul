# Pickpocket Chance Rework 1.1.0

QuickPocket compatibility is now integrated into PickpocketOverhaul.dll.
- QuickPocket previews and attempts use PCR chance calculations, Karma history, opening suspicion, failure penalties and stolen-stack penalties.
- QuickPocket remains optional; only the supported 1.0.0 binary is patched after hash and byte validation.
- Remove the old PCRQuickPocket.dll and restart. The separate StandingQuickPocket.dll handles standing hotkey access and may remain installed.
- QuickPocket transfers retain their own behavior; equipped-item drop redirection is not integrated.
- Includes the latest MCM Home SWF, duplicate-registration repair, configurable stolen-goods thresholds, migration allocation fix, and preserved hold/timers across drop/pickup.
- Runtime filenames and SKSE save identities are unchanged.

## Validation

The installed DLL, local build and release ZIP match exactly. 140,698 core checks and 35 QuickPocket native/routing checks pass; Papyrus compilation and package/export checks pass. Gameplay, QuickPocket compatibility and MCM rendering remain unverified in-game.

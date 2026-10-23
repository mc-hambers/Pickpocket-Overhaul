# Pickpocket Chance Rework 1.0.0

[Download the install ZIP](Pickpocket-Chance-Rework-v1.0.0.zip) · [Versioned release](https://github.com/mc-hambers/Pickpocket-Overhaul/releases/tag/source-1.0.0) · [Complete source archive](Pickpocket-Overhaul-source.zip)

## Current changes

- Rename the display name to Pickpocket Chance Rework; runtime filenames and serialization identity stay unchanged.
- Add selective guard confiscation and hold/value/recency-aware stolen tags.
- Contraband includes skooma variants, Sleeping Tree Sap, moon sugar, human flesh/hearts, empty/filled black soul gems, Redwater Skooma, Potion of Blood and lockpicks. Contraband Confiscation auto-disables only our classifier.
- Confiscated items go to the vanilla hold evidence chest and retain theft ownership there.
- Artful Dodger controls the hard maximum when installed; no minimum success floor is applied.
- Add difficulty point skew, updated defaults and a styled two-column MCM.
- Jewelry equipping belongs exclusively to DPL; PCR keeps the cycle API connection.
- Preserve the existing awareness, witness, distraction, bump, bad-luck protection, stolen-item tracking and optional equipped-item drop systems. Custom dialogue/confrontation features remain removed.

## Installation and settings

Requires SKSE, the matching Address Library and SkyUI. Optional integrations: Standing Pickpocket, Artful Dodger, Adamant / Hand to Hand, Dynamic Pickpocket Loot, and Contraband Confiscation.

The settings INI is generated only when absent. Existing preferences are preserved.
New defaults: karma, inventory-opening penalty, better guard removal, stolen-item
tracking and dynamic tags on; witness 1x; maximum 97%; recognition 1 day; stolen
tag base 3 days; reach 0.45x; difficulty skew 1x. Equipped-item drops remain WIP
and off by default. Gameplay logic is SKSE; MCM and recovery navigation use Papyrus.

## Source and build dependencies

The complete corresponding source, dependency sources, build scripts and original
license notices are in [Pickpocket-Overhaul-source.zip](Pickpocket-Overhaul-source.zip). Extract it before
building. The loose `src/`, tests and scripts mirror this build for browsing.
The matching precompiled CommonLib remains [CommonLibSSE.lib.xz](CommonLibSSE.lib.xz).
Use `python Restore-CommonLib.py <extracted-source-root>` to restore and verify it.
CommonLib can alternatively be rebuilt from the included source. See BUILD.md
and SOURCE-BUILD.txt for compiler/SDK/Papyrus requirements.

## Validation

140,628 behavior checks, Papyrus compilation, native build, package integrity and version metadata checks passed. **In-game testing remains pending.** Automated tests do not establish
live MCM, confiscation, equipping, AI or third-party compatibility behavior.

## License

Copyright (C) 2026 Maxmc. GPL-3.0-or-later, without warranty.
See [LICENSE.txt](LICENSE.txt). Third-party notices and applicable exceptions
are preserved in the complete source archive and runtime licensing notices.

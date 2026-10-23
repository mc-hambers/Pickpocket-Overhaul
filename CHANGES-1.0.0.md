# Pickpocket Chance Rework 1.0.0

- Rename the display name to Pickpocket Chance Rework; runtime filenames and serialization identity stay unchanged.
- Add selective guard confiscation and hold/value/recency-aware stolen tags.
- Contraband includes skooma variants, Sleeping Tree Sap, moon sugar, human flesh/hearts, empty/filled black soul gems, Redwater Skooma, Potion of Blood and lockpicks. Contraband Confiscation auto-disables only our classifier.
- Confiscated items go to the vanilla hold evidence chest and retain theft ownership there.
- Artful Dodger controls the hard maximum when installed; no minimum success floor is applied.
- Add difficulty point skew, updated defaults and a styled two-column MCM.
- Jewelry equipping belongs exclusively to DPL; PCR keeps the cycle API connection.
- Preserve the existing awareness, witness, distraction, bump, bad-luck protection, stolen-item tracking and optional equipped-item drop systems. Custom dialogue/confrontation features remain removed.

## Validation

140,628 behavior checks, Papyrus compilation, native build, package integrity and version metadata checks passed. In-game gameplay and compatibility testing remain pending.

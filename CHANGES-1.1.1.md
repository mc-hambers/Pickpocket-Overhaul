# Pickpocket Chance Rework 1.1.1

## New in 1.1.1

- Fix chance-hook initialization on Steam-protected executables: discover calls in decrypted loaded code rather than encrypted on-disk `.text`.
- Retain exact call-count, instruction-context and executable-destination validation. Theft-attempt, karma, equipped-transfer and recognition-alarm validators share one startup snapshot, released after initialization.
- Group background stolen-item inventory reads by container within each pass. Cache no inventory across frames; clear snapshots after recognition alarms. Reuse the player's hold lookup within the pass.
- Preserve 100ms bump sampling, 500ms background cadence, theft calculations, INI preferences, ESP/scripts and existing co-save formats.

Validation: native builds; 140,698 behavior checks; QuickPocket/native regression checks; 15 hook-image checks including a decrypted 1.6.1170 fixture. Actual in-game hook installation and frame-time improvement remain unverified.

Update the existing mod in Vortex, keep your generated INI, and restart Skyrim. No new save, save cleaner or Nemesis rerun is needed.

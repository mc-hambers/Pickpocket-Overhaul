# 0.1.5 runtime-aware ActorState repair
Installed 0.1.4 log: both hooks installed, reported sleeping target 0001A684 consistently read sleep-state 0/sleeping false. Formula changes cannot help when the sleeping branch is never selected.

CommonLib Actor.h inherits ActorState at the compile-time layout offset B8 in this combined SE/AE build, but AsActorState is a versioned accessor selecting C0 from runtime 1.6.629 onward. Direct inherited GetSitSleepState and IsSitting reads were incorrect for that layout. Replace all three sleep reads and sitting read with AsActorState; bump sleep exclusions are covered too. Actor::IsSneaking already uses its own runtime-aware implementation and needs no change. Vendor/companion sources remain untouched.

Chance diagnostic now includes the actual ActorState offset. The installed 1.7.104 should show C0 and a fully sleeping NPC should show internal state7/sleeping true. Sleeping numeric balance and serialization schema remain at 0.1.4. Build/math/package checks do not prove live sleep state until the new log is collected.

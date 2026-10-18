# 0.1.1 situational balance and MCM hardening

Removed skill-based cap. Independent factor strengths: awareness 50, distraction relief 20, weight 15, value 10, age/occupation 5. Skill controls execution and distraction efficiency underneath the situational ceiling. Direct undistracted observation blocks even extreme perk bonuses. Default absolute maximum 98; no minimum chance floor.

Confirmed local evidence: PickpocketOverhaul.log from 17 October reports unsupported chance call-site count 1 and no installed hooks. No full crash file was found in the local Skyrim Documents tree. MCM opening crash root cause remains unconfirmed. Papyrus now uses cached numeric readout; actor values and external distance API no longer run directly from page getters/setters. Added diagnostics rather than overriding the missing caller blindly.

Elder race identities verified read-only against Skyrim.esm record cache: ElderRace 00067CD8, ElderRaceVampire 000A82BA. Other adult ages are not inferable reliably from vanilla records. Current occupation rules distinguish guards and mapped outlaws; broader profiles remain future work.

Native build/core checks and Papyrus compile pass. Gameplay, MCM crash resolution, and the conflicting runtime hook still need live validation.

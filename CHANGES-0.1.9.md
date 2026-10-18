# 0.1.9 witness and elderly effects below the ceiling
The installed 0.1.8 log reports witness contributions (often3) while execution remains around34-40 below cap70-90. Cap-only witness and age changes can therefore leave final chances unchanged.

Witness loss now reduces raw post-perk execution as well as its ceiling. It uses AwarenessPoints times normalized witness contribution times WitnessStrength, bounded to the configured awareness loss. Strength0 disables the witness contribution, including its cap effect. Witness contributions now reflect the observer's facing/detection, sitting, scene/focus, suspicion and guard role. Existing follower, guild/friend, outlaw/shared-faction and sleeping exclusions remain.

Separate occupation risk from age relief. Keep existing occupation cap mapping; retain ElderRace/ElderRaceVampire detection, also recognize base race and four verified vanilla VTYP elderly voices: FemaleOldGrumpy00013AE2, FemaleOldKindly00013AE1, MaleOldGrumpy00013AD7, MaleOldKindly00013AD6. All signals are boolean, never stacked; no invented numerical ages or unrestricted editor-ID scans. Elderly relief is0.75 times configured profile points and affects raw execution plus the profile-adjusted cap. Default profile strength7.5 in native, INI and MCM default. Saved/custom INI choices remain user-owned.

Diagnostics show race/voice IDs, elderly flag, occupation risk, profile points, age relief, witness strength and actual witness loss. Config strength changes can trigger diagnostics even when an integer percent remains unchanged. Serialization and Papyrus MCM structure unchanged; recompile existing MCM for its new default/help text.

Tests cover low raw chances responding to witness sliders, elder detection mappings and relief, disabling age strength, configuration defaults and existing bounds/sleep behavior. Gameplay confirmation remains pending.

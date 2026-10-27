# Artful Dodger 1.11 reference inspection

Read the user-provided RAR locally. Contents: one ESP and BSA with four source scripts and their compiled PEX. The main script changes fPickPocketMaxChance on initialization, load and Pickpocket skill increase. The MCM exposes endpoints at skill 0 and skill 100. Formula: minimum + floor(skill * (maximum - minimum) / 100), with an upper bound of 100. Defaults 89 and 99; setters keep endpoints ordered.

Pickpocket Overhaul independently implements the ceiling formula, default 50/98, and applies it after its own physical-awareness calculation. It does not copy the reference scripts, load its ESP or ship its assets. The runtime does not mutate the global cap. Native UI and roll callers receive the same return value.

Reference contents are under research/artful-dodger only for local inspection, excluded from Git and all packages.

# 0.1.4 stronger sleeping assistance
The prior release was active according to the installed log, but did not log sleep recognition. Do not claim this proves which sleep state a reported NPC had.

Sleep handling help changes from 70*(1-weightBurden) to 95*(1-weightBurden^2), retaining the extra configured weight loss. Sleeping value/security and occupation cap penalties become quarter strength. Double weight cap penalties and full witness influence remain. Awake behavior and saved record schema are unchanged.

Bounded Chance diagnostics identify sleep state, sleeping boolean, skill, item weight/value, witnesses, cap and final chance. One log record at most per second on selection/state/result changes. No actor-pointer cache or growing diagnostic map.

Automated cases verify novice small-item and sleeping guard access, heavier-item penalties and witness constraints. Gameplay confirmation remains pending.

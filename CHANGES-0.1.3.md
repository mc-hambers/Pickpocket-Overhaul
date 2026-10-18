# 0.1.3 native bump, greeting and sleep implementation
Native IsSmallBump (condition 274), high-process bumpedState and bumpTimer are sampled on main-thread queued tasks every 100ms. Attribution uses recent moving-player proximity with closer actor rejection. Unknown instigator attribution is deliberately conservative; engine small-bump reaction does not guarantee any particular voiced line. No Papyrus added and no global actor state mutation.

First two distinct bumps within 60 seconds: four-second window, 24 raw misdirection. Third and later: cancel window and add 15 suspicion (capped 60). Duplicate signals within one second ignored. Baselines/contact observations are transient, cleared on save-load/new game and range exit. Existing saved attention state and unique stolen-instance records remain compatible. Broader work remains at 500ms cadence.

Outside Dialogue Menu, simultaneous native IsTalking and IsGreetingPlayer reduce attention 8. No lingering bonus and direct observation rule remains.

Sleeping: victim attention zero, up to +70 actual handling points decreasing logarithmically with item weight, an extra configured weight loss, and double cap weight penalty. Other witnesses and value/profile/max cap remain. At default settings skill15, value20 weight0 is 85%; weight5 is 31%; awake/undistracted direct-watch remains blocked.

Automated tests are not proof of native condition signals in game. Gameplay acceptance remains pending.

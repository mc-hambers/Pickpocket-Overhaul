Scriptname PO_MCM extends SKI_ConfigBase
Int[] options
Int[] debugOptions
Bool debugPage
Int debugToggle = -1
Int forceToggle = -1
Int openingToggle = -1
Int karmaToggle = -1
Int dropsToggle = -1
Int trackingToggle = -1
Int expirationToggle = -1
Int guardToggle = -1
Bool nameRefreshed = False

Function SetupPages()
    Pages = new String[3]
    Pages[0] = "Home"
    Pages[1] = "Settings"
    Pages[2] = "Debug"
EndFunction
Event OnConfigInit()
    ModName = "Pickpocket Chance Rework"
    SetupPages()
    RegisterRecovery()
EndEvent
Event OnVersionUpdate(Int version)
    ModName = "Pickpocket Chance Rework"
    SetupPages()
    RegisterRecovery()
EndEvent
Int Function GetVersion()
    Return 101
EndFunction
String Function Number(Float value)
    Int rounded
    If value < 0
        rounded = (value * 10 - 0.5) as Int
    Else
        rounded = (value * 10 + 0.5) as Int
    EndIf
    Int whole = rounded / 10
    Int digit = rounded % 10
    If digit < 0
        digit = -digit
    EndIf
    If rounded < 0 && whole == 0
        Return "-0." + digit
    EndIf
    Return whole + "." + digit
EndFunction
Function DebugRow(Int index, String label, String value)
    debugOptions[index] = AddTextOption(label, value)
EndFunction
Event OnPageReset(String page)
    PO_Native.SetTimerPreview(-1)
    If page == "" || page == "Home"
        debugPage = False
        LoadCustomContent("PickpocketOverhaul/home.dds", 24, 25)
        Return
    EndIf
    UnloadCustomContent()
    SetCursorFillMode(TOP_TO_BOTTOM)
    debugToggle = -1
    forceToggle = -1
    openingToggle = -1
    karmaToggle = -1
    dropsToggle = -1
    trackingToggle = -1
    expirationToggle = -1
    guardToggle = -1
    options = new Int[9]
    debugOptions = new Int[17]
    PO_Native.SetTimerPreview(-1)
    Int i = 0
    While i < options.Length
        options[i] = -1
        i += 1
    EndWhile
    i = 0
    While i < debugOptions.Length
        debugOptions[i] = -1
        i += 1
    EndWhile
    debugPage = page == "Debug"
    If !debugPage
        AddHeaderOption("<font color='#D4AF68'>CHANCE & AWARENESS</font>")
        karmaToggle = AddToggleOption("Karma chance mode", PO_Native.GetKarmaMode())
        openingToggle = AddToggleOption("Inventory opening penalty", PO_Native.GetOpeningPenalty())
        SetCursorPosition(1)
        AddHeaderOption("<font color='#D4AF68'>POSSESSIONS & OWNERSHIP</font>")
        If PO_Native.IsEquippedDropsReady()
            dropsToggle = AddToggleOption("Equipped-item drops", PO_Native.GetEquippedDrops())
        Else
            dropsToggle = AddToggleOption("Equipped-item drops", False, OPTION_FLAG_DISABLED)
        EndIf
        guardToggle = AddToggleOption("Better guard stolen goods removal", PO_Native.GetBetterGuard())
        trackingToggle = AddToggleOption("Stolen-item tracking", PO_Native.GetStolenTracking())
        expirationToggle = AddToggleOption("Dynamic stolen tags", PO_Native.GetStolenTagExpiration())
        SetCursorPosition(6)
        options[0] = AddSliderOption("Witness strength", PO_Native.GetOption(6), "{1}x")
        SetCursorPosition(13)
        AddTextOption("Contraband handled by", PO_Native.GetContrabandProvider())
        SetCursorPosition(15)
        options[1] = AddSliderOption("Stolen tag base days", PO_Native.GetOption(7), "{1}")
        SetCursorPosition(8)
        If PO_Native.IsCapManaged()
            options[2] = AddSliderOption("Maximum chance", PO_Native.GetOption(9), "{0}%", OPTION_FLAG_DISABLED)
        Else
            options[2] = AddSliderOption("Maximum chance", PO_Native.GetOption(9), "{0}%")
        EndIf
        If PO_Native.IsCapManaged()
            AddTextOption("Overridden by", PO_Native.GetCapSource())
        Else
            AddTextOption("Overridden by", "None")
        EndIf
        SetCursorPosition(17)
        options[3] = AddSliderOption("Item recognition base days", PO_Native.GetOption(8), "{1}")
        SetCursorPosition(12)
        options[4] = AddSliderOption("Pickpocket distance multiplier", PO_Native.GetOption(2), "{2}x")
        options[5] = AddSliderOption("Difficulty point skew", PO_Native.GetOption(10), "{1}x")
        SetCursorPosition(21)
        AddHeaderOption("<font color='#D4AF68'>STOLEN GOODS VALUES</font>")
        options[8] = AddSliderOption("Minimum stolen value", PO_Native.GetOption(13), "{0} gold")
        options[7] = AddSliderOption("Valuable stolen goods", PO_Native.GetOption(12), "{0} gold")
        options[6] = AddSliderOption("High-value stolen goods", PO_Native.GetOption(11), "{0} gold")
        Return
    EndIf
    AddHeaderOption("<font color='#80C9DA'>DEBUG & DIAGNOSTICS</font>")
    forceToggle = AddToggleOption("Force 100% pickpocket chance", PO_Native.GetForceChance100())
    debugToggle = AddToggleOption("Show witness percentage", PO_Native.GetDebugMode())
    AddHeaderOption("Last inspection")
    Float[] values = PO_Native.GetDebugValues()
    String[] names = PO_Native.GetDebugNames()
    If values != None && values.Length == 20 && names != None && names.Length == 2
        DebugRow(0, "Last inspected NPC", names[0])
        DebugRow(1, "Last inspected item", names[1])
        DebugRow(2, "Chance / ceiling", (values[0] as Int) + "% / " + Number(values[1]) + "%")
        DebugRow(3, "Skill / starting ceiling", Number(values[2]) + " / " + Number(values[3]) + "%")
        DebugRow(4, "Execution after perks", Number(values[4]) + "%")
    Else
        AddTextOption("Last inspected item", "Inspect a pickpocket item first")
    EndIf
    If PO_Native.IsStandingDistanceOwned()
        DebugRow(5, "Standing Pickpocket distance", "Managed by this mod")
    Else
        DebugRow(5, "Standing Pickpocket distance", "Integration inactive")
    EndIf
    If PO_Native.IsReady()
        DebugRow(6, "Chance hooks", "Ready")
    Else
        DebugRow(6, "Chance hooks", "Unavailable - check log")
    EndIf
    If values == None || values.Length != 20
        Return
    EndIf
    SetCursorPosition(1)
    AddHeaderOption("Calculation breakdown")
    DebugRow(7, "Awareness loss", Number(values[5]))
    DebugRow(8, "Witness cap / execution loss", Number(values[6]) + " / " + Number(values[16]))
    DebugRow(9, "Weight / value loss", Number(values[7]) + " / " + Number(values[8]))
    DebugRow(10, "Age / occupation cap change", Number(values[9]))
    DebugRow(11, "Distraction cap boost", Number(values[10]))
    DebugRow(12, "Skill cap boost", Number(values[11]))
    DebugRow(13, "Sleep / age execution boost", Number(values[12]) + " / " + Number(values[17]))
    DebugRow(14, "Bump strength / seconds left", Number(values[13]) + "% / " + Number(values[14]))
    DebugRow(16, "Stolen stacks / chance retained", (values[18] as Int) + " / " + Number(values[19]) + "%")
    If values[15] > 0
        DebugRow(15, "Recent failed theft", "80% reduction active")
    Else
        DebugRow(15, "Recent failed theft", "No penalty")
    EndIf
EndEvent
Int Function FindOption(Int option)
    If options == None || debugPage
        Return -1
    EndIf
    Int i = 0
    While i < options.Length
        If options[i] == option && option >= 0
            Return i
        EndIf
        i += 1
    EndWhile
    Return -1
EndFunction
Int Function NativeIndex(Int index)
    If index >= 6 && index <= 8
        Return index + 5
    EndIf
    If index == 0
        Return 6
    ElseIf index == 1
        Return 7
    ElseIf index == 2
        Return 9
    ElseIf index == 3
        Return 8
    ElseIf index == 5
        Return 10
    ElseIf index == 4
        Return 2
    EndIf
    Return -1
EndFunction
Event OnOptionSliderOpen(Int option)
    Int i = FindOption(option)
    If i < 0
        Return
    EndIf
    If i == 2 && PO_Native.IsCapManaged()
        Return
    EndIf
    SetSliderDialogStartValue(PO_Native.GetOption(NativeIndex(i)))
    If i >= 6
        If i == 6
            SetSliderDialogDefaultValue(300)
        ElseIf i == 7
            SetSliderDialogDefaultValue(100)
        Else
            SetSliderDialogDefaultValue(20)
        EndIf
        SetSliderDialogRange(0, 10000)
        SetSliderDialogInterval(1)
    ElseIf i == 0
        SetSliderDialogDefaultValue(1)
        SetSliderDialogRange(0, 3)
        SetSliderDialogInterval(0.1)
    ElseIf i == 1
        SetSliderDialogDefaultValue(3)
        SetSliderDialogRange(0.1, 30)
        SetSliderDialogInterval(0.1)
    ElseIf i == 2
        SetSliderDialogDefaultValue(97)
        SetSliderDialogRange(0, 100)
        SetSliderDialogInterval(1)
    ElseIf i == 3
        SetSliderDialogDefaultValue(1)
        SetSliderDialogRange(0.1, 90)
        SetSliderDialogInterval(0.1)
    ElseIf i == 5
        SetSliderDialogDefaultValue(1)
        SetSliderDialogRange(0, 3)
        SetSliderDialogInterval(0.1)
    ElseIf i == 4
        SetSliderDialogDefaultValue(0.45)
        SetSliderDialogRange(0.1, 1)
        SetSliderDialogInterval(0.05)
    EndIf
EndEvent
Event OnOptionSliderAccept(Int option, Float value)
    Int i = FindOption(option)
    If i >= 0
        PO_Native.SetOption(NativeIndex(i), value)
        ForcePageReset()
    EndIf
EndEvent
Event OnOptionSelect(Int option)
    If option == guardToggle && option >= 0
        PO_Native.SetBetterGuard(!PO_Native.GetBetterGuard())
        SetToggleOptionValue(option, PO_Native.GetBetterGuard())
        Return
    EndIf
    If !debugPage && option == trackingToggle && option >= 0
        PO_Native.SetStolenTracking(!PO_Native.GetStolenTracking())
        SetToggleOptionValue(option, PO_Native.GetStolenTracking())
        Return
    EndIf
    If !debugPage && option == expirationToggle && option >= 0
        PO_Native.SetStolenTagExpiration(!PO_Native.GetStolenTagExpiration())
        SetToggleOptionValue(option, PO_Native.GetStolenTagExpiration())
        Return
    EndIf
    If debugPage && option == forceToggle && option >= 0
        PO_Native.SetForceChance100(!PO_Native.GetForceChance100())
        SetToggleOptionValue(option, PO_Native.GetForceChance100())
        Return
    EndIf
    If !debugPage && option == dropsToggle && option >= 0
        If PO_Native.IsEquippedDropsReady()
            PO_Native.SetEquippedDrops(!PO_Native.GetEquippedDrops())
            SetToggleOptionValue(option, PO_Native.GetEquippedDrops())
        EndIf
        Return
    EndIf
    If !debugPage && option == karmaToggle && option >= 0
        Bool karmaEnabled = !PO_Native.GetKarmaMode()
        PO_Native.SetKarmaMode(karmaEnabled)
        SetToggleOptionValue(option, karmaEnabled)
        Return
    EndIf
    If !debugPage && option == openingToggle && option >= 0
        Bool openingEnabled = !PO_Native.GetOpeningPenalty()
        PO_Native.SetOpeningPenalty(openingEnabled)
        SetToggleOptionValue(option, openingEnabled)
        Return
    EndIf
    If debugPage && option == debugToggle && option >= 0
        Bool enabled = !PO_Native.GetDebugMode()
        PO_Native.SetDebugMode(enabled)
        SetToggleOptionValue(option, enabled)
    EndIf
EndEvent
Event OnOptionDefault(Int option)
    Int threshold = FindOption(option)
    If threshold >= 6
        Float gold = 20
        If threshold == 6
            gold = 300
        ElseIf threshold == 7
            gold = 100
        EndIf
        PO_Native.SetOption(NativeIndex(threshold), gold)
        ForcePageReset()
        Return
    EndIf
    If option == guardToggle && option >= 0
        PO_Native.SetBetterGuard(True)
        SetToggleOptionValue(option, True)
        Return
    EndIf
    If !debugPage && option == trackingToggle && option >= 0
        PO_Native.SetStolenTracking(True)
        SetToggleOptionValue(option, True)
        Return
    EndIf
    If !debugPage && option == expirationToggle && option >= 0
        PO_Native.SetStolenTagExpiration(True)
        SetToggleOptionValue(option, True)
        Return
    EndIf
    If debugPage && option == forceToggle && option >= 0
        PO_Native.SetForceChance100(False)
        SetToggleOptionValue(option, False)
        Return
    EndIf
    If !debugPage && option == dropsToggle && option >= 0
        PO_Native.SetEquippedDrops(False)
        SetToggleOptionValue(option, False)
        Return
    EndIf
    If !debugPage && option == karmaToggle && option >= 0
        PO_Native.SetKarmaMode(True)
        SetToggleOptionValue(option, True)
        Return
    EndIf
    If !debugPage && option == openingToggle && option >= 0
        PO_Native.SetOpeningPenalty(True)
        SetToggleOptionValue(option, True)
        Return
    EndIf
    If debugPage && option == debugToggle && option >= 0
        PO_Native.SetDebugMode(False)
        SetToggleOptionValue(option, False)
    EndIf
EndEvent
Event OnOptionHighlight(Int option)
    If option == guardToggle && option >= 0
        SetInfoText("Guards confiscate identifiable local stolen goods, high-value stolen goods from any hold, and contraband. Value thresholds are below. Stolen goods follow their tag fade timer. Confiscated items go to the hold evidence chest, like vanilla. Contraband Confiscation disables only our contraband checks. Unknown-origin cheap goods are retained. On by default.")
        Return
    EndIf
    PO_Native.SetTimerPreview(FindOption(option))
    If !debugPage && option == trackingToggle && option >= 0
        SetInfoText("Track new stolen items and let owners recognize worn possessions or notice losses. Off pauses existing tracking; history is retained. Vanilla theft ownership and equipped-item recovery remain active. On by default.")
        Return
    EndIf
    If !debugPage && option == expirationToggle && option >= 0
        SetInfoText("Adjust tracked stolen tags by hold, recency and value while retaining provenance. Valuable goods are identifiable longer. Tags age twice as fast in storage. Requires tracking; ground drops stay owned. On by default.")
        Return
    EndIf
    If debugPage && option == forceToggle && option >= 0
        SetInfoText("Testing only: force displayed and rolled pickpocket chance to 100%, bypassing awareness, witnesses, skill, penalties and maximum caps. Off by default. Saved to the INI.")
        Return
    EndIf
    If !debugPage && option == dropsToggle && option >= 0
        If PO_Native.IsEquippedDropsReady()
            SetInfoText("WIP: Taking an awake NPC's equipped gear immediately drops the original item. NPCs react, backtrack and try to recover it before resuming their routine. Combatants recover nearby weapons first. Sleeping victims transfer normally. Drops stay NPC-owned. Off by default.")
        Else
            SetInfoText("WIP. Unavailable: the equipped transfer hook could not be verified for this game or another mod changed it. Normal transfers remain active. Check PickpocketOverhaul.log.")
        EndIf
        Return
    EndIf
    If !debugPage && option == karmaToggle && option >= 0
        SetInfoText("Reduce unlucky streaks without penalizing lucky ones. Repeated failures gradually improve actual rolls. Success resets protection.")
        Return
    EndIf
    If !debugPage && option == openingToggle && option >= 0
        SetInfoText("Lower chances when repeatedly opening their pockets. Quick repeat openings build suspicion. Off stops new opening penalties; existing suspicion fades normally. On by default.")
        Return
    EndIf
    If debugPage && option == debugToggle && option >= 0
        SetInfoText("Show one percentage for the combined cap reduction from witnesses and the target's awareness, such as 64% (22%). Measures percentage points after cap limits. Display only. Off by default.")
        Return
    EndIf
    Int i = FindOption(option)
    If i == 6
        SetInfoText("Stolen items worth more than this amount can be recognized in any hold until their stolen tag fades. Contraband is handled separately. Default: 300 gold.")
    ElseIf i == 7
        SetInfoText("Value at which stolen items count as valuables in their original hold. They still follow the stolen-tag fade timer. Default: 100 gold.")
    ElseIf i == 8
        SetInfoText("Minimum item value for a visible stolen tag and guard confiscation. Cheaper items are ignored, except contraband. Default: 20 gold.")
    ElseIf i == 0
        SetInfoText("How strongly witnesses reduce the cap and success chance. 0 disables this penalty. Does not change bump wariness.")
    ElseIf i == 1
        SetInfoText("Base in-game days before tracked items lose their stolen tag. Valuable items take longer. Time passes twice as fast inside storage containers.")
    ElseIf i == 2 && PO_Native.IsCapManaged()
        SetInfoText("Maximum provided by the listed compatibility mod. Artful Dodger owns the hard maximum, including bumps; its minimum endpoint does not impose a minimum success chance. Situational calculations still apply.")
    ElseIf i == 2
        SetInfoText("Highest normal success chance, not a guaranteed chance. A valid bump can allow one extra percentage point, up to 100%.")
    ElseIf i == 3
        SetInfoText("Base in-game days an owner can recognize a tracked stolen item you wear. Valuable items are remembered longer.")
    ElseIf i == 5
        SetInfoText("Scales difficulty points. Lower values make pickpocketing easier; higher values make it harder. 1x keeps the normal balance. Does not raise the maximum chance or alter perks.")
    ElseIf i == 4
        SetInfoText("Scales allowed pickpocket reach. Lower values require you to be closer. Also controls Standing Pickpocket when its integration is active.")
    ElseIf debugPage && debugOptions != None
        i = 0
        While i < debugOptions.Length
            If debugOptions[i] == option && option >= 0
                DebugDescription(i)
                Return
            EndIf
            i += 1
        EndWhile
        SetInfoText("")
    Else
        SetInfoText("")
    EndIf
EndEvent
Function DebugDescription(Int index)
    If index == 0 || index == 1
        SetInfoText("The last NPC and item inspected in the pickpocket menu. These readouts are a snapshot, not a live scan.")
    ElseIf index == 2
        SetInfoText("Final success chance and its situational ceiling. A ceiling limits chance; it does not guarantee success.")
    ElseIf index == 3
        SetInfoText("Pickpocket skill and the starting ceiling before situational factors.")
    ElseIf index == 4
        SetInfoText("Item-handling chance after perks, before other execution effects and the final cap. Perks can raise this above 100 before clamping.")
    ElseIf index == 5
        SetInfoText("Whether this mod currently controls Standing Pickpocket's distance override.")
    ElseIf index == 6
        SetInfoText("Both native chance hooks must be ready. If unavailable, check PickpocketOverhaul.log.")
    ElseIf index == 7
        SetInfoText("Percentage points lost to attention and witnesses together. Witness loss below is part of this amount; do not add it twice.")
    ElseIf index == 8
        SetInfoText("Witness effect on the final cap, then its separate execution loss, in percentage points. Cap clamping can reduce the first value.")
    ElseIf index == 9
        SetInfoText("Weight and value reductions to the situational cap, in percentage points.")
    ElseIf index == 10
        SetInfoText("Age and occupation change to the cap. Positive raises it; negative lowers it.")
    ElseIf index == 11 || index == 12
        SetInfoText("Distraction or skill contribution to the cap, in percentage points. The final cap still respects its limits.")
    ElseIf index == 13
        SetInfoText("Sleep and age benefits to execution, in percentage points. These do not bypass the situational cap.")
    ElseIf index == 14
        SetInfoText("Current bump effectiveness and unpaused seconds remaining, at the time of inspection.")
    ElseIf index == 16
        SetInfoText("Successfully stolen stacks and the chance retained after their penalty. Low skill suffers much more. Applies after all bonuses, including bumps, and clears 60-120 unpaused seconds after the last stolen stack.")
    ElseIf index == 15
        SetInfoText("A failed pickpocket reduces execution and the cap by 80% for 60 unpaused seconds.")
    EndIf
EndFunction


Function RegisterRecovery()
    RegisterForModEvent("PO_RecoverEquipped", "OnRecoverEquipped")
    RegisterForModEvent("PO_WalkEquipped", "OnWalkEquipped")
EndFunction
Event OnConfigManagerReady(String eventName, String strArg, Float numArg, Form sender)
    ModName = "Pickpocket Chance Rework"
    SKI_ConfigManager manager = sender as SKI_ConfigManager
    If manager != None && !nameRefreshed
        manager.UnregisterMod(Self)
        Parent.OnConfigManagerReset(eventName, strArg, numArg, sender)
        nameRefreshed = True
    EndIf
    Parent.OnConfigManagerReady(eventName, strArg, numArg, sender)
EndEvent
Event OnConfigOpen()
    ModName = "Pickpocket Chance Rework"
    SetupPages()
    PO_Native.SetTimerPreview(-1)
    RegisterRecovery()
EndEvent
Event OnRecoverEquipped(String eventName, String strArg, Float numArg, Form sender)
    Actor owner = sender as Actor
    String ticket = strArg
    ObjectReference item = PO_Native.BeginRecovery(owner, ticket)
    If owner != None && item != None
        If PO_Native.CanLookRecovery(owner, ticket)
            owner.SetLookAt(item)
        EndIf
        Utility.Wait(2.0)
        If PO_Native.IsLookingAtRecoveryGoal(owner, item)
            owner.ClearLookAt()
        EndIf
        PO_Native.PrepareRecoveryWalk(owner, ticket)
    EndIf
EndEvent

Event OnWalkEquipped(String eventName, String strArg, Float numArg, Form sender)
    Actor owner = sender as Actor
    String ticket = strArg
    ObjectReference item = PO_Native.BeginRecoveryWalk(owner, ticket)
    If owner != None && item != None
        Bool reached = False
        If PO_Native.CanContinueRecovery(owner, ticket)
            reached = True
            If PO_Native.ShouldWalkRecovery(owner, ticket)
                reached = owner.PathToReference(item, 0.5)
            EndIf
        EndIf
        If PO_Native.IsLookingAtRecoveryGoal(owner, item)
            owner.ClearLookAt()
        EndIf
        PO_Native.FinishRecovery(owner, ticket, reached)
        PO_Native.CleanupRecoveryGoal(item)
    EndIf
EndEvent

Event OnConfigClose()
    PO_Native.SetTimerPreview(-1)
EndEvent


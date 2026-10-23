Scriptname PO_Native Hidden Native
Float Function GetOption(Int index) Global Native
Function SetOption(Int index, Float value) Global Native
Float Function GetCurrentCap() Global Native
Bool Function IsReady() Global Native
Bool Function IsStandingDistanceOwned() Global Native
Float[] Function GetDebugValues() Global Native
String[] Function GetDebugNames() Global Native
Bool Function GetDebugMode() Global Native
Function SetDebugMode(Bool enabled) Global Native
Bool Function GetOpeningPenalty() Global Native
Function SetOpeningPenalty(Bool enabled) Global Native

Bool Function GetKarmaMode() Global Native
Function SetKarmaMode(Bool enabled) Global Native

Bool Function IsCapManaged() Global Native

Bool Function GetEquippedDrops() Global Native
Function SetEquippedDrops(Bool enabled) Global Native
Bool Function IsEquippedDropsReady() Global Native

ObjectReference Function BeginRecovery(Actor owner, String ticket) Global Native
Function FinishRecovery(Actor owner, String ticket, Bool reached) Global Native

Function SetTimerPreview(Int index) Global Native
Function CleanupRecoveryGoal(ObjectReference goal) Global Native

Bool Function GetForceChance100() Global Native
Function SetForceChance100(Bool enabled) Global Native

Bool Function ShouldWalkRecovery(Actor owner, String ticket) Global Native

Bool Function GetStolenTracking() Global Native
Function SetStolenTracking(Bool enabled) Global Native
Bool Function GetStolenTagExpiration() Global Native
Function SetStolenTagExpiration(Bool enabled) Global Native

Bool Function CanContinueRecovery(Actor owner, String ticket) Global Native
Bool Function CanLookRecovery(Actor owner, String ticket) Global Native

Bool Function IsLookingAtRecoveryGoal(Actor owner, ObjectReference goal) Global Native

Function PrepareRecoveryWalk(Actor owner, String ticket) Global Native
ObjectReference Function BeginRecoveryWalk(Actor owner, String ticket) Global Native



Bool Function GetBetterGuard() Global Native
Function SetBetterGuard(Bool enabled) Global Native
String Function GetCapSource() Global Native

String Function GetContrabandProvider() Global Native

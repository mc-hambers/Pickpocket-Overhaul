"""Source contracts, not a live animation/ownership/UI test."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/Plugin.cpp').read_text();m=(r/'Data/Scripts/Source/PO_MCM.psc').read_text()
owner=s.split('RE::TESForm* dropOwner(',1)[1].split('void assignDropOwner',1)[0]
assert 'IsUnique()&&!base->IsDynamicForm()' in owner
assert 'GetCrimeFaction()' in owner and '!faction->IsDynamicForm()' in owner
assert 'kOwnershipExtra' in s and 'ground->extraList.SetOwner(owner)' in s
assert 'if(!owner)' in s and 'return forward()' in s
assert 'const bool withinReach=clearPickupReach(actor,ground)||' in s
assert 'path.failures<3' in s
assert 'playSeconds<path.readyAt||!withinReach' in s
assert 'Utility.Wait(2.0)' in m
assert 'BeginRecovery' in m and 'PathToReference(item, 0.5)' in m and 'FinishRecovery' in m
assert 'RegisterForModEvent("PO_RecoverEquipped", "OnRecoverEquipped")' in m
assert 'PO_Native.SetTimerPreview(FindOption(option))' in m
assert 'advanceJournal' in s and '_root.createTextField' in s
assert 'PO_Native.SetTimerPreview(-1)' in m
assert 'it->second.goal.get().get()' in s
assert 'CleanupRecoveryGoal(item)' in m
assert 'PlaceObjectAtMe(markerBase,false)' in s
assert 'request.generation!=session.load()' in s
assert 'actor->DropObject(object,extra,1,nullptr,nullptr)' in s
assert 'std::construct_at(result,std::move(dropped))' in s
assert 'position.z+=8' not in s
print('Recovery/UI/ownership source contracts passed; in-game behavior not tested')

assert 'ground->GetFormID()!=drop.reference' in s
assert 'Reconciled ground instance' in s
assert 'ground->GetBaseObject()->GetFormID()!=drop.form' in s

assert 'awaiting worn confirmation' in s and 'wearing confirmed' in s
assert 'equipRetries.erase(ground->GetFormID())' in s
assert 'equip->EquipObject(actor,object,extra,1,nullptr,true,true,false,false)' in s
assert 'playSeconds+30' in s and 'owner->IsInCombat()||owner->IsPathing()' in s
preload=s.split('case SKSE::MessagingInterface::kPreLoadGame:',1)[1].split('break;',1)[0]
assert 'SetDelete' not in preload

assert 'actor->PickUpObject(ground,1,false,false)' in s
assert 'sound.SetOutputModel(output)' in s and 'sound.SetObjectToFollow(actor->Get3D())' in s
assert 'recoveryWalkStalled' in s

assert 'processDeferredDrops();' in s and 'redirected immediately' in s
assert 'PrepareRecoveryWalk(owner, ticket)' in m and 'Event OnWalkEquipped' in m
assert 'api->OpenRecord(unfastenRecord,1)' in s and 'count*22' in s
assert 'api->OpenRecord(searchRecord,1)' in s and 'count*12' in s
assert 'api->OpenRecord(pauseRecord,1)' in s
assert 'running.target==goal->GetHandle()' in s
assert 'owner.SetLookAt(item)' in m and 'owner.ClearLookAt()' in m
assert 'progress.advance(playSeconds,searching)' in s

transfer=s.split('RE::ObjectRefHandle* transferEquipped(',1)[1].split('bool installEquippedTransfers',1)[0]
assert 'deferredDrops.push_back' not in transfer
assert 'dropOriginal(actor,object,extra,owner,uidBase,unique)' in transfer
assert 'marker->MoveTo(ground)' in s
assert 'PO_WalkEquipped' in s and 'BeginRecoveryWalk(owner, ticket)' in m
assert 'GetObjectVoiceType()' not in s

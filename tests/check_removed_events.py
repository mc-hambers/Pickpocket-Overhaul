from pathlib import Path
import struct
r=Path(__file__).resolve().parents[1];data=(r/'Data/PickpocketOverhaul.esp').read_bytes();p=0;forms=[]
while p<len(data):
 tag,n,flags,fid=struct.unpack_from('<4sIII',data,p)
 if tag==b'GRUP':p+=24;continue
 forms.append((tag,fid));p+=24+n
assert forms==[(b'TES4',0),(b'QUST',0x01000800),(b'PACK',0x01000803)],forms
assert (r/'Data/Seq/PickpocketOverhaul.seq').read_bytes()==struct.pack('<I',0x01000800)
s=(r/'src/Plugin.cpp').read_text();m=(r/'Data/Scripts/Source/PO_MCM.psc').read_text()
for removed in ['TheftRuntime','theftConfrontation','theftCases','theftEvent','VoiceAssets','recoveryDialogueUpdate','recoveryHuhTopic']:assert removed not in s
assert '.Say(' not in m and 'GetTheftEvent' not in m and 'OnTheftAlarm' not in m
assert 'droppedPickpocketCrime' in s and 'iCrimeGoldPickpocket' in s and 'ModCrimeGold(fine,false)' in s
assert 'originalGroundTheft(thief,reference,object,count,value,owner,warning)' in s
assert 'StartCombat' not in s and 'StopCombat' not in s
print('Removed-event/record/package scope checks passed; gameplay not tested')

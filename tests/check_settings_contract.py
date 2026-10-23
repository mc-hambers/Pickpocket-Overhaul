"""Static ownership checks; not an in-game save/load test."""
from pathlib import Path
import configparser
root=Path(__file__).resolve().parents[1]
s=(root/'src/Plugin.cpp').read_text()
ini=configparser.ConfigParser()
embedded=(root/'src/GeneratedIni.h').read_text().split('R"POINI(',1)[1].split(')POINI"',1)[0]
ini.read_string(embedded)
assert 'CREATE_NEW' in s and 'ensureSettingsFile();' in s
for key in ['StolenItemTracking','StolenTagExpiration','KarmaMode','InventoryOpeningPenalty','EquippedItemDrops','WitnessStrength','StolenTagDays','AbsoluteMaximum','RecognitionDays','DistanceMultiplier']:
 assert ini.has_option('Settings',key),key
 assert 'L"'+key+'"' in s,key
assert ini.has_option('Debug','Enabled')
for name in ['setStolenTracking','setStolenTagExpiration','setKarmaMode','setEquippedDrops','setOpeningPenalty','setDebugMode','setOption']:
 body=s.split('void '+name+'(',1)[1].split('\n}',1)[0]
 assert 'WritePrivateProfileStringW' in body,name
load=s.split('void load(SKSE::SerializationInterface* api)',1)[1].split('void message(',1)[0]
assert 'karmaMode=' not in load and 'equippedDrops=' not in load
assert 'if(api->OpenRecord(karmaRecord,3))' in s
assert 'if(api->OpenRecord(dropRecord,2))' in s
assert 'version==3' in load and 'version==2' in load and 'version==1' in load
assert 'loadSettings();timerPreview=-1;karma=BadLuckProtection{}' in s
assert s.split('SKSEPluginLoad(',1)[1].count('loadSettings();')==1
print('INI settings ownership checks passed; gameplay transfer not tested')

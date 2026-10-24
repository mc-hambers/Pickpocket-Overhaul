"""Package the preview without custom dialogue or voice assets."""
from pathlib import Path
import ctypes,hashlib,json,re,struct,subprocess,zipfile
from package_licenses import package
root=Path(__file__).resolve().parents[1]
release=root/'releases';release.mkdir(exist_ok=True)
stage=root/'build/package';dll=stage/'SKSE/Plugins/PickpocketOverhaul.dll'
assert dll.is_file()
lib=ctypes.WinDLL(str(dll.resolve()))
for symbol in ['SKSEPlugin_Load','SKSEPlugin_Query']:getattr(lib,symbol)
export=(ctypes.c_uint32*2).in_dll(lib,'SKSEPlugin_Version');assert export[1]==0x01000000
name=ctypes.string_at(ctypes.addressof(export)+8,256).split(b'\0')[0].decode();assert name=='Pickpocket Chance Rework'
result=subprocess.run([str(root/'build/OverhaulTests.exe')],capture_output=True,text=True,check=True)
checks=int(re.search(r'Passed (\d+) behavior checks',result.stdout)[1])
subprocess.run([__import__('sys').executable,str(root/'tests/check_removed_events.py')],check=True)
runtime=release/'Pickpocket-Chance-Rework-v1.0.0.zip';source=release/'Pickpocket-Chance-Rework-v1.0.0-source.zip'
report={'version':'1.0.0','x64_loader':'passed','exports':'passed','plugin_name':name,'core_checks':checks,'papyrus_compiled':True,'esp_esl_flag':True,'gameplay_tested':False,'source_tag':'source-1.0.0','ui_and_roll_live_tested':False,'reference_mod_redistributed':False,'equipped_drops_default_enabled':False,'equipped_drops_gameplay_tested':False,'runtime_papyrus_scripts':2,'custom_dialogue_removed':True,'ground_pickup_crime_gameplay_tested':False,'guard_confiscation_gameplay_tested':False,'loot_cycle_api_retained':True}
(root/'validation-results.json').write_text(json.dumps(report,indent=2)+'\n')
with zipfile.ZipFile(runtime,'w',zipfile.ZIP_DEFLATED) as z:
 z.write(dll,'SKSE/Plugins/PickpocketOverhaul.dll')
 for rel in ['PickpocketOverhaul.esp','Seq/PickpocketOverhaul.seq','Scripts/PO_MCM.pex','Scripts/PO_Native.pex','Interface/PickpocketOverhaul/home.dds']:
  file=root/'Data'/rel;assert file.is_file()
  if file.suffix=='.esp':assert struct.unpack_from('<I',file.read_bytes(),8)[0]&0x200
  z.write(file,rel)
with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED) as z:
 prefix='Pickpocket-Chance-Rework-v1.0.0/'
 for name in ['CMakeLists.txt','vcpkg.json','LICENSE.txt','README.md','BUILD.md','TESTING.md','COMPATIBILITY.md','.gitignore','validation-results.json']:
  z.write(root/name,prefix+name)
 for folder in ['src','tests','scripts','vendor/CommonLibSSE-NG','Data/Scripts/Source','third-party','cmake','tools/papyrus-headers','artwork','Data/Interface']:
  for file in (root/folder).rglob('*'):
   if file.is_file() and '.git' not in file.parts and '__pycache__' not in file.parts:z.write(file,prefix+file.relative_to(root).as_posix())
 for name in ['ARTFUL-DODGER-REFERENCE.md','chance-call-sites.json','recognition-alarm-audit.json','0.1.26-balance.md','0.1.28-equipped-drops.md','0.1.29-visibility.md','0.1.30-settings.md','0.1.31-recovery.md','0.1.32-slopes.md','0.1.34-recovery-failure.md','0.1.35-panels-navigation.md','0.1.38-ground-identity.md','0.1.42-repeat-recovery-crash.md','0.1.59-ground-crime.json','1.0.0-guard-hook.json','1.0.0-contraband-records.json']:
  z.write(root/'research'/name,prefix+'research/'+name)
 # Same dependency archives used by the native dependency installation.
 deps=root.parent/'Pickpocketing-Loot/tools/vcpkg-downloads'
 for file in deps.glob('*'):
  if file.is_file() and (file.name.endswith('.tar.gz') or file.suffix=='.patch'):z.write(file,prefix+'dependency-sources/'+file.name)
# Preserve exact dependency port recipes used by the portable build.
with zipfile.ZipFile(source,'a',zipfile.ZIP_DEFLATED) as z:
 prefix='Pickpocket-Chance-Rework-v1.0.0/'
 for name in ['spdlog','fmt','directxmath','directxtk','rapidcsv','vcpkg-cmake','vcpkg-cmake-config']:
  for file in (root.parent/'Pickpocketing-Loot/tools/vcpkg/ports'/name).rglob('*'):
   if file.is_file():z.write(file,prefix+'dependency-sources/ports/'+name+'/'+file.name)
package(runtime,source,source_url='https://github.com/mc-hambers/Pickpocket-Overhaul')
for archive in [runtime,source]:
 with zipfile.ZipFile(archive) as z:
  assert z.testzip() is None
  assert not any(n.lower().endswith('pickpocketoverhaul.ini') for n in z.namelist())
  assert not any('artful-dodger/' in n.lower() or n.lower().endswith(('.bsa','.esm','.rar')) for n in z.namelist())
with zipfile.ZipFile(runtime) as z:
 assert z.read('SKSE/Plugins/PickpocketOverhaul.dll')==dll.read_bytes()
 assert not any(n.lower().startswith('sound/voice/') for n in z.namelist())
 assert not any(n.startswith(('Docs/','Source/')) or n.endswith(('.cpp','.h','.lib','.pdb')) for n in z.namelist())
(root/'releases/SHA256.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [runtime,source]},indent=2)+'\n')
print(json.dumps(report,indent=2));print(runtime);print(source)

"""Read-only direct-call audit. Supply the runtime-specific target RVA."""
import argparse,json,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('executable',type=Path);p.add_argument('--target-rva',type=lambda s:int(s,0),required=True);a=p.parse_args()
data=a.executable.read_bytes();pe=struct.unpack_from('<I',data,0x3c)[0]
assert data[pe:pe+4]==b'PE\0\0'
machine,count=struct.unpack_from('<HH',data,pe+4);assert machine==0x8664
optional=struct.unpack_from('<H',data,pe+20)[0];sections=pe+24+optional;calls=[]
for i in range(count):
 off=sections+i*40;name=data[off:off+8].rstrip(b'\0')
 if name!=b'.text':continue
 virtual,rva,size,fileoff=struct.unpack_from('<IIII',data,off+8);raw=data[fileoff:fileoff+size]
 for j in range(len(raw)-5):
  if raw[j]==0xe8 and rva+j+5+struct.unpack_from('<i',raw,j+1)[0]==a.target_rva:calls.append(hex(rva+j))
print(json.dumps({'target_rva':hex(a.target_rva),'direct_calls':calls,'supported_count':len(calls)==2,'runtime_tested':False},indent=2))
if len(calls)!=2:raise SystemExit('Unsupported call-site count; do not assume UI/roll coverage')
